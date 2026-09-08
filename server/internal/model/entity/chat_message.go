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

// ChatMessage is the golang structure for table chat_message.
type ChatMessage struct {
	Id        int64       `json:"id"        orm:"id"         description:""`                        //
	AgentId   int64       `json:"agentId"   orm:"agent_id"   description:""`                        //
	DeviceMac string      `json:"deviceMac" orm:"device_mac" description:""`                        //
	Role      string      `json:"role"      orm:"role"       description:"'user' or 'assistant'"`   // 'user' or 'assistant'
	Content   string      `json:"content"   orm:"content"    description:""`                        //
	CreatedAt *gtime.Time `json:"createdAt" orm:"created_at" description:""`                        //
}
