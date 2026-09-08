/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "module_llm_protocol.h"
#include <hal/board/config.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cJSON.h>
#include <mooncake_log.h>
#include <atomic>
#include <map>

namespace module_llm {

namespace {

constexpr uart_port_t kUartPort   = UART_NUM_2;
constexpr int kUartRxBufSize      = 4096;
constexpr int kUartTxBufSize      = 2048;
// Guards against a corrupted/never-terminated line consuming memory forever.
// TTS responses (base64 WAV) are the largest expected single line - if real
// payloads turn out to exceed this, raise it rather than assume.
constexpr size_t kMaxLineLength = 65536;

const std::string_view _tag = "ModuleLLM";

struct PendingRequest {
    SemaphoreHandle_t done = nullptr;
    cJSON *response        = nullptr;  // set by reader_task, owned by the waiter once `done` is signaled
};

SemaphoreHandle_t _pending_mutex = nullptr;
std::map<std::string, PendingRequest *> _pending;
std::atomic<uint32_t> _next_request_id{1};
bool _initialized = false;

std::string next_request_id()
{
    return std::to_string(_next_request_id.fetch_add(1));
}

void handle_line(const std::string &line)
{
    cJSON *root = cJSON_Parse(line.c_str());
    if (!root) {
        mclog::tagWarn(_tag, "failed to parse line as json, dropping: {}", line);
        return;
    }

    cJSON *request_id_json = cJSON_GetObjectItem(root, "request_id");
    if (!request_id_json || !cJSON_IsString(request_id_json)) {
        mclog::tagWarn(_tag, "frame missing string request_id, dropping: {}", line);
        cJSON_Delete(root);
        return;
    }

    std::string request_id = request_id_json->valuestring;

    xSemaphoreTake(_pending_mutex, portMAX_DELAY);
    auto it = _pending.find(request_id);
    if (it == _pending.end()) {
        xSemaphoreGive(_pending_mutex);
        // Not something request() is currently waiting on. Either it timed
        // out already, or (once streaming is implemented) this is a
        // delta/finish frame - not handled yet, see header comment.
        mclog::tagInfo(_tag, "unmatched frame (request_id {}), dropping: {}", request_id, line);
        cJSON_Delete(root);
        return;
    }

    PendingRequest *pending = it->second;
    pending->response       = root;  // ownership transfers to the waiter in request()
    xSemaphoreGive(_pending_mutex);

    xSemaphoreGive(pending->done);
}

void reader_task(void *)
{
    std::string line_buf;
    uint8_t byte;

    while (true) {
        // Byte-at-a-time is simple and fine for this line-oriented protocol:
        // uart_read_bytes only blocks the full 100ms when the line is
        // actually idle, not while bytes are arriving. Revisit if profiling
        // ever shows this task as a bottleneck once TTS streaming is wired
        // in.
        int read = uart_read_bytes(kUartPort, &byte, 1, pdMS_TO_TICKS(100));
        if (read <= 0) {
            continue;
        }

        if (byte == '\n') {
            if (!line_buf.empty()) {
                handle_line(line_buf);
                line_buf.clear();
            }
            continue;
        }

        if (byte != '\r') {
            line_buf.push_back(static_cast<char>(byte));
        }

        if (line_buf.size() > kMaxLineLength) {
            mclog::tagError(_tag, "line exceeded {} bytes without a newline, discarding", kMaxLineLength);
            line_buf.clear();
        }
    }
}

}  // namespace

void init()
{
    if (_initialized) {
        return;
    }

    uart_config_t uart_config = {};
    uart_config.baud_rate     = MODULE_LLM_UART_BAUD_RATE;
    uart_config.data_bits     = UART_DATA_8_BITS;
    uart_config.parity        = UART_PARITY_DISABLE;
    uart_config.stop_bits     = UART_STOP_BITS_1;
    uart_config.flow_ctrl     = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk    = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(kUartPort, kUartRxBufSize, kUartTxBufSize, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(kUartPort, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(kUartPort, MODULE_LLM_UART_TX_PIN, MODULE_LLM_UART_RX_PIN, UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE));

    _pending_mutex = xSemaphoreCreateMutex();

    xTaskCreate(reader_task, "module_llm_reader", 4096, nullptr, 5, nullptr);

    _initialized = true;
    mclog::tagInfo(_tag, "uart initialized on UART{} (tx=io{}, rx=io{}, baud={})", static_cast<int>(kUartPort),
                   static_cast<int>(MODULE_LLM_UART_TX_PIN), static_cast<int>(MODULE_LLM_UART_RX_PIN),
                   MODULE_LLM_UART_BAUD_RATE);
}

Response request(std::string_view work_id, std::string_view action, std::string_view data_json, uint32_t timeout_ms)
{
    Response result;

    if (!_initialized) {
        result.error_message = "module_llm::init() was not called";
        return result;
    }

    std::string request_id = next_request_id();

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "request_id", request_id.c_str());
    cJSON_AddStringToObject(req, "work_id", std::string(work_id).c_str());
    cJSON_AddStringToObject(req, "action", std::string(action).c_str());

    if (!data_json.empty()) {
        cJSON *data = cJSON_Parse(std::string(data_json).c_str());
        if (data) {
            cJSON_AddItemToObject(req, "data", data);
        } else {
            mclog::tagWarn(_tag, "request() got invalid data_json, sending without a data field: {}", data_json);
        }
    }

    char *serialized = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!serialized) {
        result.error_message = "failed to serialize request";
        return result;
    }
    std::string line(serialized);
    cJSON_free(serialized);
    line.push_back('\n');

    PendingRequest pending;
    pending.done = xSemaphoreCreateBinary();

    xSemaphoreTake(_pending_mutex, portMAX_DELAY);
    _pending[request_id] = &pending;
    xSemaphoreGive(_pending_mutex);

    uart_write_bytes(kUartPort, line.data(), line.size());

    bool signaled = xSemaphoreTake(pending.done, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;

    xSemaphoreTake(_pending_mutex, portMAX_DELAY);
    _pending.erase(request_id);
    xSemaphoreGive(_pending_mutex);

    vSemaphoreDelete(pending.done);

    if (!signaled) {
        result.error_message = "timed out waiting for response";
        mclog::tagWarn(_tag, "request {} ({}.{}) timed out after {}ms", request_id, work_id, action, timeout_ms);
        // Narrow race: the response could have landed between the timeout
        // firing and the erase above taking the mutex. If so, free it here -
        // nothing else will.
        if (pending.response) {
            cJSON_Delete(pending.response);
        }
        return result;
    }

    cJSON *root = pending.response;
    if (!root) {
        result.error_message = "signaled with no response captured";
        return result;
    }

    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (error) {
        cJSON *code    = cJSON_GetObjectItem(error, "code");
        cJSON *message = cJSON_GetObjectItem(error, "message");
        if (code && cJSON_IsNumber(code)) {
            result.error_code = code->valueint;
        }
        if (message && cJSON_IsString(message)) {
            result.error_message = message->valuestring;
        }
    }

    result.ok = (result.error_code == 0);

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data) {
        char *data_str = cJSON_PrintUnformatted(data);
        if (data_str) {
            result.data_json = data_str;
            cJSON_free(data_str);
        }
    }

    cJSON_Delete(root);

    if (!result.ok) {
        mclog::tagWarn(_tag, "request {} ({}.{}) returned error {}: {}", request_id, work_id, action,
                       result.error_code, result.error_message);
    }

    return result;
}

bool ping()
{
    Response res = request("sys", "ping");
    return res.ok;
}

std::string list_models()
{
    Response res = request("sys", "lsmode", "", 10000);
    if (!res.ok) {
        return "";
    }
    return res.data_json;
}

}  // namespace module_llm
