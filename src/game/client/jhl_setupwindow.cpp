/***
 *
 *	johnhalflife: the small window the first start shows while it sets the mod up (jhl_setup.cpp). It needs the window
 *	parts of Windows.h that PlatformHeaders.h leaves out, so it's on its own, away from the SDK's headers.
 *
 ****/

#include "jhl_setupwindow.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// The SDK's HSPRITE (in the client's precompiled header) isn't Windows', as in PlatformHeaders.h.
#define HSPRITE WINDOWS_HSPRITE
#include <Windows.h>
#undef HSPRITE

JhlSetupWindow::JhlSetupWindow()
{
	const int width = 460, height = 80;

	// A static control as the window: it draws its own text, centred.
	HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP | WS_BORDER | WS_VISIBLE | SS_CENTER,
		(GetSystemMetrics(SM_CXSCREEN) - width) / 2, (GetSystemMetrics(SM_CYSCREEN) - height) / 2, width, height,
		nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

	if (window)
		SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

	m_Window = window;
}

JhlSetupWindow::~JhlSetupWindow()
{
	if (m_Window)
		DestroyWindow(static_cast<HWND>(m_Window));
}

void JhlSetupWindow::Show(const std::string& text)
{
	if (!m_Window)
		return;

	HWND window = static_cast<HWND>(m_Window);
	SetWindowTextA(window, ("\r\nJohn Half-Life: setting up from your games (first start)\r\n\r\n" + text).c_str());
	UpdateWindow(window);

	for (MSG message; PeekMessageW(&message, window, 0, 0, PM_REMOVE);)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}
#else
JhlSetupWindow::JhlSetupWindow() = default;
JhlSetupWindow::~JhlSetupWindow() = default;
void JhlSetupWindow::Show(const std::string&) {}
#endif
