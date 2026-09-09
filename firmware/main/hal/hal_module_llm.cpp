/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "board/config.h"
#include "utils/module_llm/module_llm_protocol.h"
#include "utils/module_llm/module_llm_gate.h"
#include <mooncake_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

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

    // Real bug hit this session: repeated reflashes across a dev session
    // each call llm.setup/asr.setup again, but StackFlow gives no way to
    // enumerate or bulk-clear old sessions, and this code never called
    // "exit" on the ones from previous boots - the module's task table
    // eventually filled up and setup calls started failing with error -21
    // ("task full", straight from the module's own error message). Fixed
    // by resetting the module's LLM server on every boot before setting
    // anything up, so stale sessions from earlier flashes never accumulate.
    // "sys.reset" ("Reset the unit") confirmed from M5Stack's own StackFlow
    // API docs, not guessed - distinct from "sys.reboot" ("Reboot the
    // system"), which is a full system reboot, not needed here.
    // First attempt at this fix (poll ping() after reset instead of waiting
    // for a real completion signal) cleared the -21 but immediately hit a
    // NEW error on llm.setup: -9 "unit call false" (M5Stack's docs list -9
    // as "Unit call failed" with no further detail on triggers). Most likely
    // explanation, not fully confirmed: ping() is answered by the module's
    // system-level responder, which can come back up before its LLM engine
    // has actually finished reinitializing - so llm.setup right after ping()
    // succeeds can race ahead of real readiness. Fixed by waiting for the
    // module's own documented "reset over" completion frame instead of a
    // proxy signal. That frame carries no "data" field, but on_unsolicited()
    // still fires its callback either way (with an empty payload) - see
    // module_llm_protocol.h's handle_line() - so it still works as a pure
    // completion signal here.
    SemaphoreHandle_t reset_done = xSemaphoreCreateBinary();
    module_llm::on_unsolicited("sys", [reset_done](const std::string &) { xSemaphoreGive(reset_done); });

    module_llm::Response reset_resp = module_llm::request("sys", "reset", "", 5000);
    if (!reset_resp.ok) {
        mclog::tagWarn(_tag, "sys.reset failed: code={} message={} - continuing anyway, setup calls below may hit \"task full\" if the table was already full",
                       reset_resp.error_code, reset_resp.error_message);
    } else {
        mclog::tagInfo(_tag, "sys.reset ok, waiting for module's reset-complete frame...");
        if (xSemaphoreTake(reset_done, pdMS_TO_TICKS(10000)) == pdTRUE) {
            mclog::tagInfo(_tag, "module confirmed reset complete");
        } else {
            mclog::tagWarn(_tag, "no reset-complete frame within 10s of sys.reset - continuing anyway");
        }
    }
    module_llm::on_unsolicited("sys", {});  // done with this one-shot signal - unregister so it doesn't linger
    vSemaphoreDelete(reset_done);

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

#if MODULE_LLM_AUDIO_SOURCE == MODULE_LLM_AUDIO_SOURCE_MODULE_MIC
    // --- Quick test, milestone 2: ASR on the module's own onboard mic ---
    // Two things being tested at once here: the new on_unsolicited()
    // dispatch path added to module_llm_protocol this session, and the read
    // that "input":"sys.pcm" means the module's OWN onboard mic (MSM421A),
    // not PCM pushed over UART from CoreS3 - M5Stack's hardware docs confirm
    // the module has its own mic/speaker, and their API docs don't document
    // any wire format for pushing external PCM at all. See the firmware
    // progress doc's milestone-2 section for the full writeup and the
    // MODULE_LLM_AUDIO_SOURCE flag in board/config.h. Setup JSON copied
    // verbatim from M5Stack's own documented example, not guessed/modified.
    module_llm::Response asr_setup = module_llm::request(
        "asr", "setup",
        R"({"model":"sherpa-ncnn-streaming-zipformer-20M-2023-02-17","response_format":"asr.utf-8","input":"sys.pcm","enoutput":true,"enkws":true,"rule1":2.4,"rule2":1.2,"rule3":30})",
        10000);
    if (!asr_setup.ok) {
        mclog::tagWarn(_tag, "asr setup failed: code={} message={}", asr_setup.error_code, asr_setup.error_message);
        return;
    }
    if (asr_setup.work_id.empty()) {
        mclog::tagWarn(_tag, "asr setup succeeded but response had no work_id - cannot receive results");
        return;
    }
    mclog::tagInfo(_tag, "asr setup ok, session work_id={} - say something near the MODULE's own mic (not CoreS3's)",
                   asr_setup.work_id);

    // ASR results are unsolicited pushes - per M5Stack's docs, once setup is
    // called with "input":"sys.pcm" the unit "operates continuously" with no
    // inference call needed - so this uses on_unsolicited() instead of
    // request(). Runs on the module_llm reader task; kept to just logging
    // for this diagnostic.
    // Gated on CoreS3's existing "polly" wake word (Jonathan's decision,
    // milestone 2) rather than the module's own separate KWS unit - see
    // module_llm_gate.h for the full reasoning. Without this, the module
    // transcribes ambient room noise continuously with no gating at all,
    // confirmed on hardware - not acceptable for a shipped device.
    module_llm::on_unsolicited(asr_setup.work_id, [](const std::string &data_json) {
        if (!module_llm_gate::is_open()) {
            mclog::tagDebug(_tag, "asr result (gated closed, ignoring): {}", data_json);
            return;
        }
        mclog::tagInfo(_tag, "asr result: {}", data_json);
    });
#else
    mclog::tagWarn(_tag, "MODULE_LLM_AUDIO_SOURCE_CORES3_MIC selected but not implemented yet - skipping ASR test");
#endif
}
