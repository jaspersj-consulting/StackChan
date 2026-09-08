/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package service

import (
	"context"

	"stackChan/internal/dao"
	"stackChan/internal/model/do"
	"stackChan/internal/model/entity"

	"github.com/gogf/gf/v2/database/gdb"
	"github.com/gogf/gf/v2/errors/gcode"
	"github.com/gogf/gf/v2/errors/gerror"
)

// RestoreDefaultAgent resets a device's bound agent back to the seeded default agent.
// Local replacement for the old flow, which fetched a template from xiaozhi.me and
// pushed its config back onto the device's existing agent record. Locally, "restore
// to default" simply means: point the device back at the default agent row.
func RestoreDefaultAgent(ctx context.Context, mac string) (bool, error) {
	if err := validateMac(mac); err != nil {
		return false, err
	}

	defaultAgent, err := GetDefaultAgent(ctx)
	if err != nil {
		return false, err
	}
	if defaultAgent == nil {
		return false, gerror.NewCode(gcode.CodeInternalError, "no default agent is configured")
	}

	_, err = dao.Device.Ctx(ctx).Where("mac = ?", mac).Data(do.Device{
		AgentId: defaultAgent.Id,
	}).Update()
	if err != nil {
		return false, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to restore default agent")
	}

	return true, nil
}

// GetDefaultAgent returns the agent row flagged as the default restore target, or nil if none is configured.
// Limit(1) is defensive: CreateAgent/UpdateAgent enforce at most one is_default=1 row, but this guards
// against ever silently picking a nondeterministic row if that invariant is ever violated some other way
// (e.g. a manual SQL edit).
func GetDefaultAgent(ctx context.Context) (*entity.Agent, error) {
	var agent entity.Agent
	err := dao.Agent.Ctx(ctx).Where("is_default = ?", 1).OrderAsc("id").Limit(1).Scan(&agent)
	if err != nil {
		return nil, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to query default agent")
	}
	if agent.Id == 0 {
		return nil, nil
	}
	return &agent, nil
}

// ListAgents returns every configured agent.
func ListAgents(ctx context.Context) ([]entity.Agent, error) {
	var agents []entity.Agent
	err := dao.Agent.Ctx(ctx).OrderAsc("id").Scan(&agents)
	if err != nil {
		return nil, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to list agents")
	}
	return agents, nil
}

// GetAgent returns a single agent by id.
func GetAgent(ctx context.Context, id int64) (*entity.Agent, error) {
	if id == 0 {
		return nil, gerror.NewCode(gcode.CodeMissingParameter, "agent id cannot be empty")
	}
	var agent entity.Agent
	err := dao.Agent.Ctx(ctx).Where("id = ?", id).Limit(1).Scan(&agent)
	if err != nil {
		return nil, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to query agent")
	}
	if agent.Id == 0 {
		return nil, gerror.NewCode(gcode.CodeNotFound, "agent not found")
	}
	return &agent, nil
}

// AgentConfigInput carries the editable fields of an agent config bundle.
type AgentConfigInput struct {
	Name           string
	AssistantName  string
	Persona        string
	LlmModel       string
	TtsVoice       string
	TtsSpeechSpeed string
	TtsPitch       int
	AsrSpeed       string
	Language       string
	Memory         string
	MemoryType     string
	IsDefault      bool
}

// clearOtherDefaults unsets is_default on every agent row other than excludeId, within tx.
// Called before inserting/updating a row with IsDefault=true, so "at most one default agent"
// stays true as an invariant instead of relying on callers getting it right. excludeId of 0
// matches no row (agent ids start at 1), which is what we want when the row being made
// default doesn't exist yet (the CreateAgent case).
func clearOtherDefaults(ctx context.Context, tx gdb.TX, excludeId int64) error {
	_, err := dao.Agent.Ctx(ctx).TX(tx).Where("id != ?", excludeId).Data(do.Agent{
		IsDefault: 0,
	}).Update()
	return err
}

// CreateAgent creates a new agent config bundle and returns its id.
func CreateAgent(ctx context.Context, input AgentConfigInput) (id int64, err error) {
	if input.Name == "" {
		return 0, gerror.NewCode(gcode.CodeMissingParameter, "agent name cannot be empty")
	}

	isDefault := 0
	if input.IsDefault {
		isDefault = 1
	}

	err = dao.Agent.Transaction(ctx, func(ctx context.Context, tx gdb.TX) error {
		if input.IsDefault {
			if err := clearOtherDefaults(ctx, tx, 0); err != nil {
				return err
			}
		}

		result, err := dao.Agent.Ctx(ctx).TX(tx).Data(do.Agent{
			Name:           input.Name,
			AssistantName:  input.AssistantName,
			Persona:        input.Persona,
			LlmModel:       input.LlmModel,
			TtsVoice:       input.TtsVoice,
			TtsSpeechSpeed: input.TtsSpeechSpeed,
			TtsPitch:       input.TtsPitch,
			AsrSpeed:       input.AsrSpeed,
			Language:       input.Language,
			Memory:         input.Memory,
			MemoryType:     input.MemoryType,
			IsDefault:      isDefault,
		}).Insert()
		if err != nil {
			return err
		}

		id, err = result.LastInsertId()
		return err
	})
	if err != nil {
		return 0, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to create agent")
	}
	return id, nil
}

// UpdateAgent updates an existing agent's config bundle.
func UpdateAgent(ctx context.Context, id int64, input AgentConfigInput) error {
	current, err := GetAgent(ctx, id)
	if err != nil {
		return err
	}
	if current.IsDefault != 0 && !input.IsDefault {
		return gerror.NewCode(gcode.CodeInvalidParameter, "cannot unset the default agent directly; set another agent as default instead, which clears this one's default flag automatically")
	}

	isDefault := 0
	if input.IsDefault {
		isDefault = 1
	}

	err = dao.Agent.Transaction(ctx, func(ctx context.Context, tx gdb.TX) error {
		if input.IsDefault {
			if err := clearOtherDefaults(ctx, tx, id); err != nil {
				return err
			}
		}

		_, err := dao.Agent.Ctx(ctx).TX(tx).Where("id = ?", id).Data(do.Agent{
			Name:           input.Name,
			AssistantName:  input.AssistantName,
			Persona:        input.Persona,
			LlmModel:       input.LlmModel,
			TtsVoice:       input.TtsVoice,
			TtsSpeechSpeed: input.TtsSpeechSpeed,
			TtsPitch:       input.TtsPitch,
			AsrSpeed:       input.AsrSpeed,
			Language:       input.Language,
			Memory:         input.Memory,
			MemoryType:     input.MemoryType,
			IsDefault:      isDefault,
		}).Update()
		return err
	})
	if err != nil {
		return gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to update agent")
	}
	return nil
}

// DeleteAgent deletes an agent. Any device bound to it falls back to no agent
// (device.agent_id is set NULL by the FK) rather than being left dangling.
// Refuses to delete the current default agent, since RestoreDefaultAgent (and
// therefore every device unbind) depends on one always existing.
func DeleteAgent(ctx context.Context, id int64) error {
	agentToDelete, err := GetAgent(ctx, id)
	if err != nil {
		return err
	}
	if agentToDelete.IsDefault != 0 {
		return gerror.NewCode(gcode.CodeInvalidParameter, "cannot delete the default agent; make another agent the default first")
	}
	_, err = dao.Agent.Ctx(ctx).Where("id = ?", id).Delete()
	if err != nil {
		return gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to delete agent")
	}
	return nil
}

// BindDeviceAgent points a device at the given agent, creating the device row if needed.
func BindDeviceAgent(ctx context.Context, mac string, agentId int64) error {
	if err := validateMac(mac); err != nil {
		return err
	}
	if _, err := GetAgent(ctx, agentId); err != nil {
		return err
	}
	if _, err := CreateMacIfNotExists(ctx, mac); err != nil {
		return err
	}

	_, err := dao.Device.Ctx(ctx).Where("mac = ?", mac).Data(do.Device{
		AgentId: agentId,
	}).Update()
	if err != nil {
		return gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to bind device to agent")
	}
	return nil
}
