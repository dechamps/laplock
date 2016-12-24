// Copyright 2011 (C) Etienne Dechamps (e-t172) <e-t172@akegroup.org>
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <sstream>

#include <windows.h>

static void systemError(wchar_t* what)
{
	static wchar_t buffer[1024];
	const wchar_t* errorMessage = buffer;

	if (!FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		0,
		GetLastError(),
		0,
		buffer,
		sizeof(buffer),
		0
	))
		errorMessage = L"(cannot format error message)";

	std::wstringstream message;
	message << L"A system error occured within laplock." << std::endl;
	message << L"Operation: " << what << std::endl;
	message << L"System message: " << errorMessage;
	MessageBox(NULL, message.str().c_str(), L"laplock error", MB_OK | MB_ICONERROR);

	exit(EXIT_FAILURE);
}

static LRESULT CALLBACK windowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg != WM_POWERBROADCAST || wParam != PBT_POWERSETTINGCHANGE)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	const POWERBROADCAST_SETTING* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
	if (setting->PowerSetting != GUID_MONITOR_POWER_ON && setting->PowerSetting != GUID_LIDSWITCH_STATE_CHANGE)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	const DWORD* state = reinterpret_cast<const DWORD*>(&setting->Data);
	if (*state != 0)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	if (LockWorkStation() == 0)
		systemError(L"locking workstation");

	return 0;
}

static void registerWindowClass(HINSTANCE instance)
{
	WNDCLASSEX windowClass = { 0 };

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = &windowProcedure;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = L"laplock";

	if (RegisterClassEx(&windowClass) == 0)
		systemError(L"registering window class");
}

static HWND createWindow(HINSTANCE instance)
{
	HWND hWnd = CreateWindow(
		L"laplock",
		NULL,
		0,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		HWND_MESSAGE,
		NULL,
		instance,
		NULL
	);

	if (hWnd == NULL)
		systemError(L"creating window");
	
	return hWnd;
}

static void registerNotification(HWND window)
{
	if (!RegisterPowerSettingNotification(window, &GUID_MONITOR_POWER_ON, DEVICE_NOTIFY_WINDOW_HANDLE))
		systemError(L"cannot register GUID_MONITOR_POWER_ON power setting notification");
	if (!RegisterPowerSettingNotification(window, &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE))
		systemError(L"cannot register GUID_LIDSWITCH_STATE_CHANGE power setting notification");
}

static WPARAM messageLoop()
{
	for (;;)
	{
		MSG message;
		BOOL result = GetMessage(&message, NULL, 0, 0);
		if (result == -1)
			systemError(L"getting window message");
		if (result == 0)
			return message.wParam;
		DispatchMessage(&message);
	}
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
	registerWindowClass(instance);

	HWND window = createWindow(instance);

	registerNotification(window);

	return messageLoop();
}
