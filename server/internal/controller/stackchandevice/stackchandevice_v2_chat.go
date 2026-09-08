/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package stackchandevice

import (
	"context"
	"stackChan/internal/model"
	"stackChan/internal/service"

	"github.com/gogf/gf/v2/errors/gcode"
	"github.com/gogf/gf/v2/errors/gerror"
	"github.com/gogf/gf/v2/frame/g"

	"stackChan/api/stackchandevice/v2"
)

// Chat sends a transcribed user utterance to the device's bound agent (or the
// default agent if none is bound), gets a text reply from the configured LLM
// backend (Ollama), and returns it. Introduced for Path #2 (Module LLM handles
// wake-word/ASR/TTS on-device; only text crosses the network here).
func (c *ControllerV2) Chat(ctx context.Context, req *v2.ChatReq) (res *v2.ChatRes, err error) {
	mac := g.RequestFromCtx(ctx).GetCtxVar(model.Mac).String()
	if mac == "" {
		return nil, gerror.NewCodef(gcode.CodeInvalidParameter, "Device MAC address is empty")
	}

	reply, err := service.ChatWithAgent(ctx, mac, req.Text)
	if err != nil {
		return nil, err
	}

	return &v2.ChatRes{Text: reply}, nil
}
