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

func (c *ControllerV1) DeleteAgent(ctx context.Context, req *v1.DeleteAgentReq) (res *v1.DeleteAgentRes, err error) {
	err = service.DeleteAgent(ctx, req.Id)
	if err != nil {
		return nil, err
	}
	result := v1.DeleteAgentRes(true)
	return &result, nil
}
