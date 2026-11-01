/***
 *
 *	johnhalflife: the small window the first start shows while it sets the mod up (jhl_setup.cpp). Windows only; a
 *	window that can't be made, or another platform, shows nothing.
 *
 ****/

#pragma once

#include <string>

class JhlSetupWindow
{
public:
	JhlSetupWindow();
	~JhlSetupWindow();

	JhlSetupWindow(const JhlSetupWindow&) = delete;
	JhlSetupWindow& operator=(const JhlSetupWindow&) = delete;

	/**
	 *	@brief Shows what the setup is doing now, under its title.
	 */
	void Show(const std::string& text);

private:
	void* m_Window = nullptr;
};
