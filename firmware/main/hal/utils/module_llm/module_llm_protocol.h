/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

// Low-level StackFlow protocol client for the M5Stack Module LLM (AX630C),
// connected over UART. StackFlow is a line-delimited JSON request/response
// protocol - see docs.m5stack.com/en/stackflow/module_llm/api.
//
// This layer implements the synchronous request/response half of the
// protocol (used for setup/action calls and one-shot queries like sys.ping
// and sys.lsmode) via request(), plus a callback-based path for unsolicited
// push frames via on_unsolicited() - see that function's comment for what
// counts as "unsolicited" (ASR results, KWS detections).
//
// NOT yet handled: a single request() call that gets multiple response
// frames sharing its request_id before a final one (e.g. an
// "llm.utf-8.stream" inference, whose intermediate frames carry
// {"delta","index","finish":false} and the last carries "finish":true).
// request() today returns on the FIRST frame matching its request_id, so a
// streaming call would return early with just the first delta, and later
// frames for that same request_id would arrive after the pending entry is
// already erased - they currently fall through to the "unmatched frame"
// drop path, same as anything else with no matching pending request or
// on_unsolicited handler. See the firmware progress doc's Module LLM section
// for what's still outstanding.
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

// Registers a callback for unsolicited frames belonging to `work_id` - frames
// that arrive with no matching pending request(), which is how the module
// pushes continuous results once a unit is put in a listening state:
// ASR result frames after "asr.setup" (input "sys.pcm" - the module streams
// results with no per-chunk inference call needed, confirmed from M5Stack's
// StackFlow docs; object "asr.stream"/"asr.utf-8.stream", frame's own
// work_id e.g. "asr.1003", request_id is one the module picked, not one we
// issued), and KWS detection frames after "kws.setup" (module docs are
// explicit KWS has no inference action - it just pushes events).
//
// `callback` receives the raw JSON text of the frame's "data" field. It runs
// on the reader task, so it must be fast and non-blocking - queue the real
// work, don't do it inline. Only one callback per work_id; registering again
// replaces the previous one. Pass an empty std::function to unregister.
void on_unsolicited(std::string_view work_id, std::function<void(const std::string &data_json)> callback);

// sys.ping - true if the module responded with error.code == 0.
bool ping();

// sys.lsmode - the response's raw "data" JSON (installed models/units) as
// text, or an empty string on failure/timeout.
std::string list_models();

}  // namespace module_llm
