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

func (c *ControllerV1) BindDeviceAgent(ctx context.Context, req *v1.BindDeviceAgentReq) (res *v1.BindDeviceAgentRes, err error) {
	err = service.BindDeviceAgent(ctx, req.Mac, req.AgentId)
	if err != nil {
		return nil, err
	}
	result := v1.BindDeviceAgentRes(true)
	return &result, nil
}
