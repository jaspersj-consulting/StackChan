/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

// =================================================================================
// Code generated and maintained by GoFrame CLI tool. DO NOT EDIT.
// =================================================================================

package agent

import (
	"context"

	"stackChan/api/agent/v1"
)

type IAgentV1 interface {
	GetAgent(ctx context.Context, req *v1.GetAgentReq) (res *v1.GetAgentRes, err error)
	ListAgents(ctx context.Context, req *v1.ListAgentsReq) (res *v1.ListAgentsRes, err error)
	CreateAgent(ctx context.Context, req *v1.CreateAgentReq) (res *v1.CreateAgentRes, err error)
	UpdateAgent(ctx context.Context, req *v1.UpdateAgentReq) (res *v1.UpdateAgentRes, err error)
	DeleteAgent(ctx context.Context, req *v1.DeleteAgentReq) (res *v1.DeleteAgentRes, err error)
	BindDeviceAgent(ctx context.Context, req *v1.BindDeviceAgentReq) (res *v1.BindDeviceAgentRes, err error)
}
