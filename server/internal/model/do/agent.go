/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

// =================================================================================
// Code generated and maintained by GoFrame CLI tool. DO NOT EDIT.
// =================================================================================

package do

import (
	"github.com/gogf/gf/v2/frame/g"
	"github.com/gogf/gf/v2/os/gtime"
)

// Agent is the golang structure of table agent for DAO operations like Where/Data.
type Agent struct {
	g.Meta         `orm:"table:agent, do:true"`
	Id             any         //
	Name           any         //
	AssistantName  any         //
	Persona        any         //
	LlmModel       any         //
	TtsVoice       any         //
	TtsSpeechSpeed any         //
	TtsPitch       any         //
	AsrSpeed       any         //
	Language       any         //
	Memory         any         //
	MemoryType     any         //
	IsDefault      any         //
	CreatedAt      *gtime.Time //
	UpdatedAt      *gtime.Time //
}
