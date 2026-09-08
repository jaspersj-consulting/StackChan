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

func (c *ControllerV1) UpdateAgent(ctx context.Context, req *v1.UpdateAgentReq) (res *v1.UpdateAgentRes, err error) {
	err = service.UpdateAgent(ctx, req.Id, service.AgentConfigInput{
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
	result := v1.UpdateAgentRes(true)
	return &result, nil
}
