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

// AgentDao is the data access object for the table agent.
type AgentDao struct {
	table    string
	group    string
	columns  AgentColumns
	handlers []gdb.ModelHandler
}

// AgentColumns defines and stores column names for the table agent.
type AgentColumns struct {
	Id             string
	Name           string
	AssistantName  string
	Persona        string
	LlmModel       string
	TtsVoice       string
	TtsSpeechSpeed string
	TtsPitch       string
	AsrSpeed       string
	Language       string
	Memory         string
	MemoryType     string
	IsDefault      string
	CreatedAt      string
	UpdatedAt      string
}

var agentColumns = AgentColumns{
	Id:             "id",
	Name:           "name",
	AssistantName:  "assistant_name",
	Persona:        "persona",
	LlmModel:       "llm_model",
	TtsVoice:       "tts_voice",
	TtsSpeechSpeed: "tts_speech_speed",
	TtsPitch:       "tts_pitch",
	AsrSpeed:       "asr_speed",
	Language:       "language",
	Memory:         "memory",
	MemoryType:     "memory_type",
	IsDefault:      "is_default",
	CreatedAt:      "created_at",
	UpdatedAt:      "updated_at",
}

// NewAgentDao creates and returns a new DAO object for table data access.
func NewAgentDao(handlers ...gdb.ModelHandler) *AgentDao {
	return &AgentDao{
		group:    "default",
		table:    "agent",
		columns:  agentColumns,
		handlers: handlers,
	}
}

// DB retrieves and returns the underlying raw database management object of the current DAO.
func (dao *AgentDao) DB() gdb.DB {
	return g.DB(dao.group)
}

// Table returns the table name of the current DAO.
func (dao *AgentDao) Table() string {
	return dao.table
}

// Columns returns all column names of the current DAO.
func (dao *AgentDao) Columns() AgentColumns {
	return dao.columns
}

// Group returns the database configuration group name of the current DAO.
func (dao *AgentDao) Group() string {
	return dao.group
}

// Ctx creates and returns a Model for the current DAO. It automatically sets the context for the current operation.
func (dao *AgentDao) Ctx(ctx context.Context) *gdb.Model {
	model := dao.DB().Model(dao.table)
	for _, handler := range dao.handlers {
		model = handler(model)
	}
	return model.Safe().Ctx(ctx)
}

// Transaction wraps the transaction logic using function f.
// It rolls back the transaction and returns the error if function f returns a non-nil error.
// It commits the transaction and returns nil if function f returns nil.
func (dao *AgentDao) Transaction(ctx context.Context, f func(ctx context.Context, tx gdb.TX) error) (err error) {
	return dao.Ctx(ctx).Transaction(ctx, f)
}
