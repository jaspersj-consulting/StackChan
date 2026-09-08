/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

// =================================================================================
// Code generated and maintained by GoFrame CLI tool. DO NOT EDIT.
// =================================================================================

package entity

import (
	"github.com/gogf/gf/v2/os/gtime"
)

// Agent is the golang structure for table agent.
type Agent struct {
	Id             int64       `json:"id"             orm:"id"                description:""`                                          //
	Name           string      `json:"name"           orm:"name"              description:"Agent config bundle name"`                  // Agent config bundle name
	AssistantName  string      `json:"assistantName"  orm:"assistant_name"    description:"Spoken assistant name"`                     // Spoken assistant name
	Persona        string      `json:"persona"        orm:"persona"           description:"Character / system-prompt text"`            // Character / system-prompt text
	LlmModel       string      `json:"llmModel"       orm:"llm_model"         description:""`                                          //
	TtsVoice       string      `json:"ttsVoice"       orm:"tts_voice"         description:""`                                          //
	TtsSpeechSpeed string      `json:"ttsSpeechSpeed" orm:"tts_speech_speed"  description:""`                                          //
	TtsPitch       int         `json:"ttsPitch"       orm:"tts_pitch"         description:""`                                          //
	AsrSpeed       string      `json:"asrSpeed"       orm:"asr_speed"         description:""`                                          //
	Language       string      `json:"language"       orm:"language"         description:""`                                          //
	Memory         string      `json:"memory"         orm:"memory"            description:""`                                          //
	MemoryType     string      `json:"memoryType"     orm:"memory_type"       description:""`                                          //
	IsDefault      int         `json:"isDefault"      orm:"is_default"        description:"Which row RestoreDefaultAgent resets to"`   // Which row RestoreDefaultAgent resets to
	CreatedAt      *gtime.Time `json:"createdAt"      orm:"created_at"        description:""`                                          //
	UpdatedAt      *gtime.Time `json:"updatedAt"      orm:"updated_at"        description:""`                                          //
}
