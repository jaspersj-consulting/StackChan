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
}
