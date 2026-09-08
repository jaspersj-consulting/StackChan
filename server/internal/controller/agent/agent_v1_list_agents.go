/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package agent

import (
	"context"

	"stackChan/internal/service"

	"stackChan/api/agent/v1"
)

func (c *ControllerV1) ListAgents(ctx context.Context, req *v1.ListAgentsReq) (res *v1.ListAgentsRes, err error) {
	agents, err := service.ListAgents(ctx)
	if err != nil {
		return nil, err
	}
	result := v1.ListAgentsRes(agents)
	return &result, nil
}
