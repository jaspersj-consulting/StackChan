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

// ChatMessage is the golang structure of table chat_message for DAO operations like Where/Data.
type ChatMessage struct {
	g.Meta    `orm:"table:chat_message, do:true"`
	Id        any         //
	AgentId   any         //
	DeviceMac any         //
	Role      any         //
	Content   any         //
	CreatedAt *gtime.Time //
}
