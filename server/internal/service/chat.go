/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package service

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"net/http"
	"time"

	"stackChan/internal/dao"
	"stackChan/internal/model/do"
	"stackChan/internal/model/entity"

	"github.com/gogf/gf/v2/errors/gcode"
	"github.com/gogf/gf/v2/errors/gerror"
	"github.com/gogf/gf/v2/frame/g"
)

// chatHistoryLimit is how many prior turns (user+assistant messages combined)
// get replayed as context on each request. Kept small deliberately for the
// first cut - no summarization/trimming strategy yet.
const chatHistoryLimit = 10

type ollamaMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type ollamaChatRequest struct {
	Model    string           `json:"model"`
	Messages []ollamaMessage  `json:"messages"`
	Stream   bool             `json:"stream"`
}

type ollamaChatResponse struct {
	Message struct {
		Role    string `json:"role"`
		Content string `json:"content"`
	} `json:"message"`
	Error string `json:"error"`
}

// ChatWithAgent sends a transcribed user utterance to the device's bound agent
// (falling back to the default agent if the device has none bound), calls the
// configured Ollama instance for a response, and persists both turns to
// chat_message. Returns the assistant's reply text.
//
// This is Path #2's "Option B" integration: the Module LLM handles wake-word,
// ASR, and TTS entirely on-device (see claude/stackchan-firmware-progress.md),
// so only already-transcribed text ever reaches this endpoint - no audio
// streaming, no OTA/websocket protocol involved.
func ChatWithAgent(ctx context.Context, mac string, text string) (string, error) {
	if err := validateMac(mac); err != nil {
		return "", err
	}
	if text == "" {
		return "", gerror.NewCode(gcode.CodeMissingParameter, "text cannot be empty")
	}

	agent, err := resolveDeviceAgent(ctx, mac)
	if err != nil {
		return "", err
	}

	llmModel := agent.LlmModel
	if llmModel == "" {
		llmModel = g.Cfg().MustGet(ctx, "ollama.default_model").String()
	}
	if llmModel == "" {
		return "", gerror.NewCode(gcode.CodeInternalError, "no LLM model configured: set the agent's llm_model or ollama.default_model")
	}

	baseUrl := g.Cfg().MustGet(ctx, "ollama.base_url").String()
	if baseUrl == "" {
		return "", gerror.NewCode(gcode.CodeInternalError, "ollama.base_url is not configured")
	}

	messages := make([]ollamaMessage, 0, chatHistoryLimit+2)
	if agent.Persona != "" {
		messages = append(messages, ollamaMessage{Role: "system", Content: agent.Persona})
	}

	history, err := recentChatHistory(ctx, agent.Id, mac, chatHistoryLimit)
	if err != nil {
		return "", err
	}
	for _, m := range history {
		messages = append(messages, ollamaMessage{Role: m.Role, Content: m.Content})
	}
	messages = append(messages, ollamaMessage{Role: "user", Content: text})

	reply, err := callOllamaChat(ctx, baseUrl, llmModel, messages)
	if err != nil {
		return "", err
	}

	if err := recordChatTurn(ctx, agent.Id, mac, "user", text); err != nil {
		return "", err
	}
	if err := recordChatTurn(ctx, agent.Id, mac, "assistant", reply); err != nil {
		return "", err
	}

	return reply, nil
}

// resolveDeviceAgent returns the agent bound to this device, or the default
// agent if the device has none bound yet (mirrors RestoreDefaultAgent's
// fallback semantics).
func resolveDeviceAgent(ctx context.Context, mac string) (*entity.Agent, error) {
	var device entity.Device
	err := dao.Device.Ctx(ctx).Where("mac = ?", mac).Scan(&device)
	if err != nil {
		return nil, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to query device")
	}

	if device.AgentId != 0 {
		return GetAgent(ctx, device.AgentId)
	}

	defaultAgent, err := GetDefaultAgent(ctx)
	if err != nil {
		return nil, err
	}
	if defaultAgent == nil {
		return nil, gerror.NewCode(gcode.CodeInternalError, "no default agent is configured")
	}
	return defaultAgent, nil
}

// recentChatHistory returns up to `limit` prior turns for this agent+device,
// oldest first (chronological order, suitable for replay to the LLM).
func recentChatHistory(ctx context.Context, agentId int64, mac string, limit int) ([]entity.ChatMessage, error) {
	var rows []entity.ChatMessage
	err := dao.ChatMessage.Ctx(ctx).
		Where("agent_id = ? AND device_mac = ?", agentId, mac).
		OrderDesc("id").
		Limit(limit).
		Scan(&rows)
	if err != nil {
		return nil, gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to load chat history")
	}
	for i, j := 0, len(rows)-1; i < j; i, j = i+1, j-1 {
		rows[i], rows[j] = rows[j], rows[i]
	}
	return rows, nil
}

func recordChatTurn(ctx context.Context, agentId int64, mac, role, content string) error {
	_, err := dao.ChatMessage.Ctx(ctx).Data(do.ChatMessage{
		AgentId:   agentId,
		DeviceMac: mac,
		Role:      role,
		Content:   content,
	}).Insert()
	if err != nil {
		return gerror.WrapCode(gcode.CodeDbOperationError, err, "failed to record chat turn")
	}
	return nil
}

// callOllamaChat calls Ollama's /api/chat endpoint (non-streaming) and
// returns the assistant message content.
func callOllamaChat(ctx context.Context, baseUrl, model string, messages []ollamaMessage) (string, error) {
	reqBody := ollamaChatRequest{
		Model:    model,
		Messages: messages,
		Stream:   false,
	}
	payload, err := json.Marshal(reqBody)
	if err != nil {
		return "", gerror.WrapCode(gcode.CodeInternalError, err, "failed to marshal ollama request")
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, baseUrl+"/api/chat", bytes.NewReader(payload))
	if err != nil {
		return "", gerror.WrapCode(gcode.CodeInternalError, err, "failed to build ollama request")
	}
	httpReq.Header.Set("Content-Type", "application/json")

	// 240s: this Mac Mini's qwen3:14b measured throughput is ~6.8 tok/s
	// generation (~170s for a large prompt) - a shorter timeout would kill
	// real, correctly-working requests as false failures.
	client := &http.Client{Timeout: 240 * time.Second}
	httpRes, err := client.Do(httpReq)
	if err != nil {
		return "", gerror.WrapCode(gcode.CodeInternalError, err, "failed to reach ollama")
	}
	defer httpRes.Body.Close()

	body, err := io.ReadAll(httpRes.Body)
	if err != nil {
		return "", gerror.WrapCode(gcode.CodeInternalError, err, "failed to read ollama response")
	}

	if httpRes.StatusCode != http.StatusOK {
		return "", gerror.NewCodef(gcode.CodeInternalError, "ollama returned status %d: %s", httpRes.StatusCode, string(body))
	}

	var ollamaRes ollamaChatResponse
	if err := json.Unmarshal(body, &ollamaRes); err != nil {
		return "", gerror.WrapCode(gcode.CodeInternalError, err, "failed to parse ollama response")
	}
	if ollamaRes.Error != "" {
		return "", gerror.NewCodef(gcode.CodeInternalError, "ollama error: %s", ollamaRes.Error)
	}
	if ollamaRes.Message.Content == "" {
		return "", gerror.NewCode(gcode.CodeInternalError, "ollama returned an empty response")
	}

	return ollamaRes.Message.Content, nil
}
