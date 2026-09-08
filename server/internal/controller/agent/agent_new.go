/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package agent

import (
	"stackChan/api/agent"
)

type ControllerV1 struct{}

func NewV1() agent.IAgentV1 {
	return &ControllerV1{}
}
