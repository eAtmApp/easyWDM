// trayicondemo.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "easyDualMonitor.h"

#include "tray_icon.h"
#include "dualMonitor.h"

#include <easy/easy.h>
using namespace easy;

#pragma comment(lib, EASY_LIB_NAME("easylib"))

#define MID_DESKTOP 1
#define MID_RUN 2
#define MID_HOOK_WND 3
#define MID_LIMIT 4

struct _CONFIG_INFO
{
	bool show_desktop = true;
	bool show_run = true;
	bool limit_mouse = true;
	bool move_windows = true;

	void readConfig()
	{
		jsoncpp json;
		set_json(show_desktop);
		set_json(show_run);
		set_json(limit_mouse);
		set_json(move_windows);
		json.readConfig();
		
		get_json_bool(show_desktop);
		get_json_bool(show_run);
		get_json_bool(limit_mouse);
		get_json_bool(move_windows);
	}
	bool saveConfig()
	{
		jsoncpp json;
		set_json(show_desktop);
		set_json(show_run);
		set_json(limit_mouse);
		set_json(move_windows);
		return json.writeConfig();
	}
};

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	tray_icon tray(IDI_TRAYICONDEMO, "easyWDM");

	_CONFIG_INFO cfg;

	cfg.readConfig();
	
	dualMonitor dualMon(tray);

	//防止重复运行
	{
		auto hMutex = ::CreateMutex(nullptr, FALSE, "easy Dual Monitor");
		if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS)
		{
			::MessageBoxA(::GetDesktopWindow(), "此程序不允许重复运行!", "easy Dual Monitor", MB_OK | MB_ICONWARNING);
			return 1;
		}
	}

	auto showDesktop_ = std::bind(&dualMonitor::ShowDisktop, &dualMon);
	auto showRunDlg_ = std::bind(&dualMonitor::ShowRunDlg, &dualMon);
	
	tray.add_menu("启用[显示桌面]", MID_DESKTOP, [&]()
		{
			if (tray.is_menu_checked(MID_DESKTOP))
			{
				tray.set_menu_checked(MID_DESKTOP, false);
				tray.unregister_hotkey(MOD_WIN, 'D');
				cfg.show_desktop = false;
			}
			else {
				if (tray.register_hotkey(MOD_WIN, 'D', showDesktop_))
				{
					tray.set_menu_checked(MID_DESKTOP, true);
					cfg.show_desktop = true;
				}
			}
			cfg.saveConfig();
		});
	tray.add_menu("启用[运行窗口]", MID_RUN, [&]()
		{
			if (tray.is_menu_checked(MID_RUN))
			{
				tray.set_menu_checked(MID_RUN, false);
				tray.unregister_hotkey(MOD_WIN, 'R');
				cfg.show_run = false;
			}
			else {
				if (tray.register_hotkey(MOD_WIN, 'R', showRunDlg_))
				{
					tray.set_menu_checked(MID_RUN, true);
					cfg.show_run = true;
				}
			}
			cfg.saveConfig();
		});
	tray.add_menu("启用[窗口转移]", MID_HOOK_WND, [&]()
		{
			if (tray.is_menu_checked(MID_HOOK_WND))
			{
				dualMon.hookWnd(false);
				tray.set_menu_checked(MID_HOOK_WND, false);
				cfg.move_windows = false;
			}
			else {
				dualMon.hookWnd(true);
				tray.set_menu_checked(MID_HOOK_WND, true);
				cfg.move_windows = true;
			}
			cfg.saveConfig();
		});
	tray.add_menu("启用[限制鼠标]", MID_LIMIT, [&]()
		{
			if (tray.is_menu_checked(MID_LIMIT))
			{
				dualMon.limitMouse(false);
				tray.set_menu_checked(MID_LIMIT, false);
				cfg.limit_mouse = false;
			}
			else {
				if (dualMon.limitMouse(true))
				{
					tray.set_menu_checked(MID_LIMIT, true);
					cfg.limit_mouse = true;
				}
			}
			cfg.saveConfig();
		});

	tray.add_separator();
	tray.add_menu("退出(&X)", [&]()
		{
			tray.close();
		});

	if (cfg.show_desktop)
	{
		if (tray.register_hotkey(MOD_WIN, 'D', showDesktop_))
		{
			tray.set_menu_checked(MID_DESKTOP, true);
		}
		else {
			cfg.show_desktop = false;
		}
	}

	if (cfg.show_run)
	{
		if (tray.register_hotkey(MOD_WIN, 'R', showRunDlg_))
		{
			tray.set_menu_checked(MID_RUN, true);
		}
		else {
			cfg.show_run = false;
		}
	}

	if (cfg.move_windows)
	{
		if (dualMon.hookWnd(true))
		{
			tray.set_menu_checked(MID_HOOK_WND, true);
		}
		else {
			cfg.move_windows = false;
		}
	}

	if (cfg.limit_mouse)
	{
		if (dualMon.limitMouse(true))
		{
			tray.set_menu_checked(MID_LIMIT, true);
		}
		else {
			cfg.limit_mouse = false;
		}
	}

	cfg.saveConfig();
	
	tray.run();
	return 0;

}