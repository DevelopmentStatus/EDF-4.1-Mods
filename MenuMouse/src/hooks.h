#pragma once

#include <windows.h>

namespace mm {

// Mouse state sampled once per game frame, in game-window client pixels.
struct MouseFrame {
	POINT client{};      // cursor position in the game window's client area
	SIZE clientSize{};   // client area size, for scaling into UI space
	bool inside = false; // cursor is inside the client area
	bool moved = false;  // moved since the previous frame
	bool leftPressed = false;
	bool rightPressed = false;
	int wheelSteps = 0;  // +1 per notch up, -1 per notch down
};

bool InstallInputHooks();
void RemoveInputHooks();

// Called by the menu code at most once per frame.
MouseFrame PollMouse();

// While set, the game's own relative mouse handling is muted and the
// cursor is left free and visible.
void SetMenuMouseMode(bool active);

HWND GameWindow();

} // namespace mm
