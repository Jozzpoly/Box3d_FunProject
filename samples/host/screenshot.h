// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// Captures the current D3D11 backbuffer (the frame just committed by the host,
// including the ImGui panel) to a PNG file. Windows/D3D11 only; a no-op that
// returns false on other backends. Driven by the samples.exe --screenshot flag
// so rig/visual work can be verified from the ACTUAL engine render instead of a
// hand-drawn proxy - the render-is-the-gate tooling the M8 repair needs.
bool CaptureFrameToPng( const char* path );
