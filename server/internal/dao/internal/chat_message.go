/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

// ==========================================================================
// Code generated and maintained by GoFrame CLI tool. DO NOT EDIT.
// ==========================================================================

package internal

import (
	"context"

	"github.com/gogf/gf/v2/database/gdb"
	"github.com/gogf/gf/v2/frame/g"
)

// ChatMessageDao is the data access object for the table chat_message.
type ChatMessageDao struct {
	table    string
	group    string
	columns  ChatMessageColumns
	handlers []gdb.ModelHandler
}

// ChatMessageColumns defines and stores column names for the table chat_message.
type ChatMessageColumns struct {
	Id        string
	AgentId   string
	DeviceMac string
	Role      string
	Content   string
	CreatedAt string
}

var chatMessageColumns = ChatMessageColumns{
	Id:        "id",
	AgentId:   "agent_id",
	DeviceMac: "device_mac",
	Role:      "role",
	Content:   "content",
	CreatedAt: "created_at",
}

// NewChatMessageDao creates and returns a new DAO object for table data access.
func NewChatMessageDao(handlers ...gdb.ModelHandler) *ChatMessageDao {
	return &ChatMessageDao{
		group:    "default",
		table:    "chat_message",
		columns:  chatMessageColumns,
		handlers: handlers,
	}
}

// DB retrieves and returns the underlying raw database management object of the current DAO.
func (dao *ChatMessageDao) DB() gdb.DB {
	return g.DB(dao.group)
}

// Table returns the table name of the current DAO.
func (dao *ChatMessageDao) Table() string {
	return dao.table
}

// Columns returns all column names of the current DAO.
func (dao *ChatMessageDao) Columns() ChatMessageColumns {
	return dao.columns
}

// Group returns the database configuration group name of the current DAO.
func (dao *ChatMessageDao) Group() string {
	return dao.group
}

// Ctx creates and returns a Model for the current DAO. It automatically sets the context for the current operation.
func (dao *ChatMessageDao) Ctx(ctx context.Context) *gdb.Model {
	model := dao.DB().Model(dao.table)
	for _, handler := range dao.handlers {
		model = handler(model)
	}
	return model.Safe().Ctx(ctx)
}

// Transaction wraps the transaction logic using function f.
// It rolls back the transaction and returns the error if function f returns a non-nil error.
// It commits the transaction and returns nil if function f returns nil.
func (dao *ChatMessageDao) Transaction(ctx context.Context, f func(ctx context.Context, tx gdb.TX) error) (err error) {
	return dao.Ctx(ctx).Transaction(ctx, f)
}
