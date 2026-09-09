/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <string>
#include <string_view>

// Low-level StackFlow protocol client for the M5Stack Module LLM (AX630C),
// connected over UART. StackFlow is a line-delimited JSON request/response
// protocol - see docs.m5stack.com/en/stackflow/module_llm/api.
//
// This layer only implements the synchronous request/response half of the
// protocol (used for setup/action calls and one-shot queries like sys.ping
// and sys.lsmode). Streaming responses (ASR/LLM "delta" frames sent before a
// "finish" frame) are not yet handled here - an incoming frame whose
// request_id isn't currently awaited by request() is logged and dropped.
// See the firmware progress doc's Module LLM section for what's still
// outstanding (this is intentionally the first milestone, not the full
// integration).
namespace module_llm {

struct Response {
    bool ok = false;  // true if the module's response had error.code == 0
    int error_code    = 0;
    std::string error_message;
    std::string data_json;  // raw JSON text of the response's "data" field (empty if absent/null)
    // The response's own "work_id" field. For a "setup" call this is a NEW
    // session id (e.g. "llm.1003") that must be used for that unit's
    // follow-up calls instead of the generic work_id ("llm") used to create
    // it - confirmed from M5Stack's StackFlow API examples. Empty if the
    // response had no work_id (shouldn't happen per the documented frame
    // shape, but not assumed).
    std::string work_id;
};

// Brings up the UART peripheral (pins/baud from hal/board/config.h) and
// starts the background reader task. Safe to call once; later calls are
// no-ops. Does not talk to the module - call ping() afterwards to verify
// it's actually there and answering.
void init();

// Sends a StackFlow request ({"request_id","work_id","action"[,"data"]}) and
// blocks for the matching response (matched by request_id) or until
// timeout_ms elapses. `data_json`, if non-empty, must be a valid JSON value
// (object/array/string/etc.) and becomes the request's "data" field.
Response request(std::string_view work_id, std::string_view action, std::string_view data_json = "",
                  uint32_t timeout_ms = 5000);

// sys.ping - true if the module responded with error.code == 0.
bool ping();

// sys.lsmode - the response's raw "data" JSON (installed models/units) as
// text, or an empty string on failure/timeout.
std::string list_models();

}  // namespace module_llm
