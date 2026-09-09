/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

// A tiny, dependency-free open/closed flag gating whether the Module LLM's
// ASR output should be treated as real input.
//
// Why this exists: the Module LLM's ASR (see hal_module_llm.cpp) transcribes
// continuously once asr.setup is called with "input":"sys.pcm" - M5Stack's
// own docs say it "operates continuously," and enkws:true in the setup JSON
// does NOT gate this on its own (confirmed on hardware this session: ASR
// fired unprompted on ambient room conversation with no wake word
// configured). An always-transcribing mic is not acceptable for a shipped
// device, so this flag is checked before the ASR on_unsolicited() callback
// acts on a result.
//
// Gating decision (Jonathan, milestone 2): reuse CoreS3's existing,
// already-verified MultiNet "polly" custom wake word rather than the Module
// LLM's own separate KWS unit (kws.setup's unsolicited-frame shape isn't
// documented by M5Stack at all, so it isn't a quick add). xiaozhi-esp32's
// Application opens the gate the moment "polly" is detected from idle
// (Application::HandleWakeWordDetectedEvent()) and closes it when the
// device returns to idle (Application::HandleStateChangedEvent()'s
// kDeviceStateIdle case) - the same window CoreS3's own legacy
// tenclass.net conversation uses. That hook lives in
// xiaozhi-esp32/main/application.cc, a patched/vendored/gitignored
// dependency - the change is tracked via firmware/patches/xiaozhi-esp32.patch,
// not this repo's normal git history.
//
// Deliberately free of FreeRTOS/UART/StackFlow specifics so it can be
// included from application.cc without pulling in the rest of the Module
// LLM protocol stack.
namespace module_llm_gate
{
    // Called when CoreS3's wake word fires from idle - starts a window
    // during which Module LLM ASR results are treated as real input.
    void open();

    // Called when the device returns to idle - ends that window.
    void close();

    // Checked by the ASR on_unsolicited() callback before acting on a
    // result. Safe to call from any task.
    bool is_open();
} // namespace module_llm_gate
