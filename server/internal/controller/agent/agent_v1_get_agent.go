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

func (c *ControllerV1) GetAgent(ctx context.Context, req *v1.GetAgentReq) (res *v1.GetAgentRes, err error) {
	a, err := service.GetAgent(ctx, req.Id)
	if err != nil {
		return nil, err
	}
	result := v1.GetAgentRes(*a)
	return &result, nil
}
