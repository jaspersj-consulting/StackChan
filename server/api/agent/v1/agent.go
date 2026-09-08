/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package v1

import (
	"stackChan/internal/model/entity"

	"github.com/gogf/gf/v2/frame/g"
)

type GetAgentReq struct {
	g.Meta `path:"/agent" method:"get" tags:"Agent" summary:"Get a single agent by id"`
	Id     int64 `json:"id" v:"required" dc:"Agent id"`
}

type GetAgentRes entity.Agent

type ListAgentsReq struct {
	g.Meta `path:"/agents" method:"get" tags:"Agent" summary:"List all configured agents"`
}

type ListAgentsRes []entity.Agent

type CreateAgentReq struct {
	g.Meta         `path:"/agent" method:"post" tags:"Agent" summary:"Create a new agent config bundle"`
	Name           string `json:"name" v:"required" dc:"Agent config bundle name"`
	AssistantName  string `json:"assistantName" dc:"Spoken assistant name"`
	Persona        string `json:"persona" dc:"Character / system-prompt text"`
	LlmModel       string `json:"llmModel" dc:"LLM model identifier"`
	TtsVoice       string `json:"ttsVoice" dc:"TTS voice identifier"`
	TtsSpeechSpeed string `json:"ttsSpeechSpeed" dc:"TTS speech speed"`
	TtsPitch       int    `json:"ttsPitch" dc:"TTS pitch"`
	AsrSpeed       string `json:"asrSpeed" dc:"ASR speed"`
	Language       string `json:"language" dc:"Language code"`
	Memory         string `json:"memory" dc:"Persisted memory text"`
	MemoryType     string `json:"memoryType" dc:"Memory mode, e.g. OFF"`
	IsDefault      bool   `json:"isDefault" dc:"Whether this is the default restore-to agent"`
}

type CreateAgentRes struct {
	Id int64 `json:"id"`
}

type UpdateAgentReq struct {
	g.Meta         `path:"/agent" method:"put" tags:"Agent" summary:"Update an agent config bundle"`
	Id             int64  `json:"id" v:"required" dc:"Agent id"`
	Name           string `json:"name" v:"required" dc:"Agent config bundle name"`
	AssistantName  string `json:"assistantName" dc:"Spoken assistant name"`
	Persona        string `json:"persona" dc:"Character / system-prompt text"`
	LlmModel       string `json:"llmModel" dc:"LLM model identifier"`
	TtsVoice       string `json:"ttsVoice" dc:"TTS voice identifier"`
	TtsSpeechSpeed string `json:"ttsSpeechSpeed" dc:"TTS speech speed"`
	TtsPitch       int    `json:"ttsPitch" dc:"TTS pitch"`
	AsrSpeed       string `json:"asrSpeed" dc:"ASR speed"`
	Language       string `json:"language" dc:"Language code"`
	Memory         string `json:"memory" dc:"Persisted memory text"`
	MemoryType     string `json:"memoryType" dc:"Memory mode, e.g. OFF"`
	IsDefault      bool   `json:"isDefault" dc:"Whether this is the default restore-to agent"`
}

type UpdateAgentRes bool

type DeleteAgentReq struct {
	g.Meta `path:"/agent" method:"delete" tags:"Agent" summary:"Delete an agent config bundle"`
	Id     int64 `json:"id" v:"required" dc:"Agent id"`
}

type DeleteAgentRes bool

type BindDeviceAgentReq struct {
	g.Meta  `path:"/device/agent/bind" method:"post" tags:"Agent" summary:"Bind a device to an agent"`
	Mac     string `json:"mac" v:"required" dc:"Device MAC address"`
	AgentId int64  `json:"agentId" v:"required" dc:"Agent id to bind the device to"`
}

type BindDeviceAgentRes bool
