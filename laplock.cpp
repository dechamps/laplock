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

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

#include <windows.h>

namespace laplock {
namespace {

class LogLine {
public:
	LogLine() {
		if (!logfile) return;
		auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		tm localnow;
		if (localtime_s(&localnow, &now) == 0) {
			*logfile << std::put_time(&localnow, L"%F %T ");
		}
	}

	~LogLine() {
		if (!logfile) return;
		*logfile << std::endl;
		logfile->flush();
	}

	template <typename T> LogLine& operator<<(T value) {
		if (!logfile) return *this;
		*logfile << value;
		return *this;
	}

	static std::unique_ptr<std::wofstream> logfile;
};

std::unique_ptr<std::wofstream> LogLine::logfile;

std::wostream& operator<<(std::wostream& stream, const GUID& guid) {
	wchar_t str[128] = { 0 };
	StringFromGUID2(guid, str, sizeof(str));
	stream << str;
	return stream;
}

static void systemError(wchar_t* what)
{
	const DWORD error = GetLastError();
	LogLine() << "Error " << error << " during: " << what;
	static wchar_t buffer[1024];
	const wchar_t* errorMessage = buffer;

	if (!FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		0,
		error,
		0,
		buffer,
		sizeof(buffer),
		0
	))
		errorMessage = L"(cannot format error message)";
	else
		LogLine() << "System error message: " << errorMessage;

	std::wstringstream message;
	message << L"A system error occured within laplock." << std::endl;
	message << L"Operation: " << what << std::endl;
	message << L"System message: " << errorMessage;
	MessageBox(NULL, message.str().c_str(), L"laplock error", MB_OK | MB_ICONERROR);

	exit(EXIT_FAILURE);
}

static LRESULT CALLBACK windowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg != WM_POWERBROADCAST || wParam != PBT_POWERSETTINGCHANGE) {
		LogLine() << "Window received irrelevant message";
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	if (GetSystemMetrics(SM_REMOTESESSION)) {
		LogLine() << "Ignoring window message because session is currently remote";
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	const POWERBROADCAST_SETTING* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
	LogLine() << "Received POWERBROADCAST_SETTING " << setting->PowerSetting;
	if (setting->PowerSetting != GUID_MONITOR_POWER_ON && setting->PowerSetting != GUID_LIDSWITCH_STATE_CHANGE) {
		LogLine() << "Received irrelevant POWERBROADCAST_SETTING";
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	const DWORD* state = reinterpret_cast<const DWORD*>(&setting->Data);
	LogLine() << "POWERBROADCAST_SETTING state: " << *state;
	if (*state != 0) {
		LogLine() << "Irrelevant POWERBROADCAST_SETTING state";
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	LogLine() << "Locking";
	if (LockWorkStation() == 0)
		systemError(L"locking workstation");
	else
		LogLine() << "Locked";

	return 0;
}

static void registerWindowClass(HINSTANCE instance)
{
	LogLine() << "Registering window class";
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
	LogLine() << "Creating window";
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
	LogLine() << "Registering GUID_MONITOR_POWER_ON (GUID: " << GUID_MONITOR_POWER_ON << ")";
	if (!RegisterPowerSettingNotification(window, &GUID_MONITOR_POWER_ON, DEVICE_NOTIFY_WINDOW_HANDLE))
		systemError(L"cannot register GUID_MONITOR_POWER_ON power setting notification");
	LogLine() << "Registering GUID_LIDSWITCH_STATE_CHANGE (GUID: " << GUID_LIDSWITCH_STATE_CHANGE << ")";
	if (!RegisterPowerSettingNotification(window, &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE))
		systemError(L"cannot register GUID_LIDSWITCH_STATE_CHANGE power setting notification");
}

static WPARAM messageLoop()
{
	for (;;)
	{
		LogLine() << "Awaiting next window message";
		MSG message;
		BOOL result = GetMessage(&message, NULL, 0, 0);
		if (result == -1)
			systemError(L"getting window message");
		if (result == 0)
			return message.wParam;
		LogLine() << "Dispatching message";
		DispatchMessage(&message);
	}
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int)
{
	if (*commandLine != 0)
		LogLine::logfile.reset(new std::wofstream(commandLine, std::ios_base::app));
	LogLine() << "laplock initializing";

	registerWindowClass(instance);

	HWND window = createWindow(instance);

	registerNotification(window);

	WPARAM result = messageLoop();
	LogLine() << "laplock terminating";
	return result;
}

}
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int cmdShow)
{
	return laplock::WinMain(instance, prevInstance, commandLine, cmdShow);
}
