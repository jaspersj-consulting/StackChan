/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "module_llm_gate.h"

#include <atomic>

namespace
{
    // Two independent tasks touch this: xiaozhi-esp32's Application task
    // (open()/close(), driven by CoreS3's wake word/state machine) and the
    // module_llm UART reader task (is_open(), checked from the ASR
    // on_unsolicited() callback). A single atomic bool is sufficient - this
    // is a plain flag being read and written, not a multi-step invariant
    // that needs a mutex.
    std::atomic<bool> _gate_open{false};
} // namespace

namespace module_llm_gate
{
    void open()
    {
        _gate_open.store(true, std::memory_order_relaxed);
    }

    void close()
    {
        _gate_open.store(false, std::memory_order_relaxed);
    }

    bool is_open()
    {
        return _gate_open.load(std::memory_order_relaxed);
    }
} // namespace module_llm_gate
