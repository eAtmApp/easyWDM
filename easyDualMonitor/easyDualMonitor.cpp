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
#define MID_MOVE_WND 3
#define MID_LIMIT 4
#define MID_AUTO_RUN 5

static constexpr auto app_name = "easyWDM";

struct _CONFIG_INFO
{
	bool show_desktop = false;
	bool limit_mouse = true;
	bool move_windows = true;
	bool autorun = false;

	void readConfig()
	{
		jsoncpp json;
		set_json(show_desktop);
		set_json(limit_mouse);
		set_json(move_windows);
		set_json(autorun);
		json.readConfig();

		get_json_bool(show_desktop);
		get_json_bool(limit_mouse);
		get_json_bool(move_windows);
		get_json_bool(autorun);
	}
	bool saveConfig()
	{
		jsoncpp json;
		set_json(show_desktop);
		set_json(limit_mouse);
		set_json(move_windows);
		set_json(autorun);
		return json.writeConfig();
	}
};

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	eStringV svCmd(lpCmdLine ? lpCmdLine : "");

	if (process.exe_name().compare_icase("startMenu") || svCmd.compare_icase("startMenu"))
	{
		dualMonitor::show_StartMenu();
		return 0;
	}

	process.set_app_name(app_name);

	tray_icon tray(IDI_TRAYICONDEMO, app_name);

	_CONFIG_INFO cfg;

	cfg.readConfig();

	dualMonitor dualMon(tray);

	//防止重复运行
	{
		if (process.is_already_run())
		{
			::MessageBoxA(::GetDesktopWindow(), "此程序不允许重复运行!", "easy Dual Monitor", MB_OK | MB_ICONWARNING);
			return 1;
		}
	}

	tray.AddMenu("替换[Win+D]", MID_DESKTOP, [&]()
		{
			bool enable = !tray.GetCheck();

			if (dualMon.replace_ShowDesktop(enable))
			{
				tray.SetCheck(enable);
				cfg.show_desktop = enable;
				cfg.saveConfig();
			}
		});

	tray.AddMenu("启用[窗口转移]", MID_MOVE_WND, [&]()
		{
			auto enable = !tray.GetCheck();
			if (dualMon.hook_wnd(enable))
			{
				tray.SetCheck(enable);
				cfg.move_windows = enable;
				cfg.saveConfig();
			}
		});
	tray.AddMenu("启用[鼠标限制]", MID_LIMIT, [&]()
		{
			auto enable = !tray.GetCheck();
			if (dualMon.set_limit_mouse(enable))
			{
				tray.SetCheck(enable);
				cfg.limit_mouse = enable;
				cfg.saveConfig();
			}
		});
	tray.AddSeparator();
	tray.AddMenu("开机自动运行", MID_AUTO_RUN, [&]()
		{
			bool is_enable = !tray.GetCheck();
			if (process.set_autorun(is_enable))
			{
				tray.SetCheck(is_enable);
				cfg.autorun = is_enable;
				cfg.saveConfig();
			}
		});
	tray.AddSeparator();
	tray.AddMenu("退出(&X)", [&]()
		{
			tray.close();
		});


	if (cfg.show_desktop)
	{
		if (dualMon.replace_ShowDesktop(cfg.show_desktop))
		{
			cfg.show_desktop = true;
			tray.SetCheck(MID_DESKTOP, true);
		}
		else {
			cfg.show_desktop = false;
		}
	}


	if (cfg.move_windows && dualMon.hook_wnd(true))
	{
		tray.SetCheck(MID_MOVE_WND, true);
	}
	else {
		cfg.move_windows = false;
	}


	if (dualMon.set_limit_mouse(cfg.limit_mouse))
	{
		tray.SetCheck(MID_LIMIT, true);
	}
	else {
		cfg.limit_mouse = false;
	}
	
	if (process.set_autorun(cfg.autorun))
	{
		tray.SetCheck(MID_AUTO_RUN, cfg.autorun);
	}
	else {
		cfg.autorun = false;
	}

	cfg.saveConfig();

	tray.run();
	return 0;

}