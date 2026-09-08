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

func (c *ControllerV1) CreateAgent(ctx context.Context, req *v1.CreateAgentReq) (res *v1.CreateAgentRes, err error) {
	id, err := service.CreateAgent(ctx, service.AgentConfigInput{
		Name:           req.Name,
		AssistantName:  req.AssistantName,
		Persona:        req.Persona,
		LlmModel:       req.LlmModel,
		TtsVoice:       req.TtsVoice,
		TtsSpeechSpeed: req.TtsSpeechSpeed,
		TtsPitch:       req.TtsPitch,
		AsrSpeed:       req.AsrSpeed,
		Language:       req.Language,
		Memory:         req.Memory,
		MemoryType:     req.MemoryType,
		IsDefault:      req.IsDefault,
	})
	if err != nil {
		return nil, err
	}
	return &v1.CreateAgentRes{Id: id}, nil
}
