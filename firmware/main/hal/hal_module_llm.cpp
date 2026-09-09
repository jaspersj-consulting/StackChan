/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "utils/module_llm/module_llm_protocol.h"
#include <mooncake_log.h>

static const std::string_view _tag = "HAL-ModuleLLM";

void Hal::module_llm_init()
{
    mclog::tagInfo(_tag, "init");

    module_llm::init();

    // First-milestone wiring test: confirm the module answers on the wire at
    // all, and log what it currently has installed (models/units, via
    // sys.lsmode). Neither call blocks the rest of boot for long even if the
    // module never answers (unpowered, wrong pins, not yet flashed with
    // StackFlow) - this is a boot-time diagnostic, not a hard dependency.
    if (!module_llm::ping()) {
        mclog::tagWarn(_tag, "module did not respond to sys.ping - check UART wiring/power, skipping sys.lsmode");
        return;
    }
    mclog::tagInfo(_tag, "module responded to sys.ping");

    std::string models = module_llm::list_models();
    if (models.empty()) {
        mclog::tagWarn(_tag, "sys.lsmode returned no data");
    } else {
        mclog::tagInfo(_tag, "sys.lsmode: {}", models);
    }

    // --- Quick test, ahead of milestone 2: one-shot non-streaming LLM chat ---
    // Confirms the module can actually reason and reply over UART, not just
    // report its installed capabilities, before investing in the
    // streaming/audio-routing work milestone 2 needs for ASR/TTS/KWS. Uses
    // the module's non-streaming response_format ("llm.utf-8", not
    // ".stream"), which per M5Stack's own StackFlow API examples returns the
    // complete reply in a single frame - so this reuses the existing
    // synchronous request() exactly like ping()/list_models() above, with no
    // new protocol-handling code. Field values mirror M5Stack's documented
    // working example as closely as possible (model name, enkws, etc.)
    // rather than guessing at what changing them would do.
    module_llm::Response llm_setup = module_llm::request(
        "llm", "setup",
        R"({"model":"qwen2.5-0.5B-prefill-20e","response_format":"llm.utf-8","input":"llm.utf-8","enoutput":true,"enkws":true,"max_token_len":127,"prompt":"You are a concise cooking assistant. Answer in one short sentence."})",
        10000);
    if (!llm_setup.ok) {
        mclog::tagWarn(_tag, "llm setup failed: code={} message={}", llm_setup.error_code, llm_setup.error_message);
        return;
    }
    if (llm_setup.work_id.empty()) {
        mclog::tagWarn(_tag, "llm setup succeeded but response had no work_id - cannot send inference");
        return;
    }
    mclog::tagInfo(_tag, "llm setup ok, session work_id={}", llm_setup.work_id);

    // Per the API doc, follow-up calls for this session must use the NEW
    // work_id from the setup response (e.g. "llm.1003"), not the generic
    // "llm" work_id used to create it.
    module_llm::Response llm_reply =
        module_llm::request(llm_setup.work_id, "inference", R"("What temperature should I sear a steak at?")", 15000);
    if (!llm_reply.ok) {
        mclog::tagWarn(_tag, "llm inference failed: code={} message={}", llm_reply.error_code, llm_reply.error_message);
        return;
    }
    // data_json here is the response's "data" field re-serialized as JSON
    // text, so it will print with surrounding quotes/escapes (e.g.
    // "\"For searing a steak, preheat...\"") rather than plain text - fine
    // for this diagnostic, not worth unescaping for a one-shot boot test.
    mclog::tagInfo(_tag, "llm reply: {}", llm_reply.data_json);
}
