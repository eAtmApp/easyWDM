#include "easyWDM.h"
#include "helper.hpp"

easyWDM::easyWDM(tray_icon& tray)
	:_tray(tray)
{
	m_pThis = this;
}

easyWDM::~easyWDM()
{
	UninstallHook();
}

void easyWDM::UninstallHook()
{
	if (g_Shell_Wnd_Msg_ID)
	{
		DeregisterShellHookWindow(_tray.GetWnd());
		g_Shell_Wnd_Msg_ID = 0;
	}

	if (_hookKey)
	{
		UnhookWindowsHookEx(_hookKey);
		_hookKey = nullptr;
	}

	if (_hookMouse)
	{
		UnhookWindowsHookEx(_hookMouse);
		_hookKey = nullptr;
	}
}

bool easyWDM::initConfig()
{
	_config["hook_key"] = true;
	_config["hook_mouse"] = false;
	_config["hook_windows"] = true;
	_config["hotkey"] = {};
	auto& win = _config["hotkey"];
	win["win+c"] = {
			{"type","open"},
			{"path","cmd.exe"},
			{"dir","c:\\"}
	};
	win["win+z"] = {
			{"type","open"},
			{"path","calc.exe"},
	};

	win["win+e"] = {
		{"type","open"},
		{"path","explorer.exe"},
		{"param","::{20D04FE0-3AEA-1069-A2D8-08002B30309D}"}
	};
	win["ctrl"] = {
		{"type","msg"},
		{"param","这是ctrl"}
	};
	win["ctrl+b"] = {
		{"type","msg"},
		{"param","测试左右"}
	};

	win["alt"] = {
		{"type","msg"},
		{"param","这是alt"}
	};

	if (!_config.readConfig())
	{
		process.exit("读取配置文件失败,如格式错乱,删除配置文件即可恢复默认!");
		return false;
	}

	//内置功能
	SetHotkey("win+d", helper::ShowDisktop);
	SetHotkey("win+r", std::bind(helper::ShowRunDlg, false));
	SetHotkey("win", helper::show_StartMenu);
	
	jsoncpp& hotkey = _config["hotkey"];
	hotkey.forEach([&](std::string_view key, jsoncpp& item)
		{
			bool result = SetHotkey(std::string(key), [&item]()
				{
					etd::string optype = item["type"];
					optype.tolower();
					if (optype == "open")	//打开应用或文档
					{
						etd::string_view path = item["path"];
						etd::string_view param = item["param"];
						etd::string_view dir = item["dir"];

						auto result = (UINT)ShellExecuteA(nullptr,
							optype.empty() ? nullptr : optype.data(),
							path.empty() ? nullptr : path.data(),
							param.empty() ? nullptr : param.data(),
							dir.empty() ? nullptr : dir.data(),
							SW_SHOWNORMAL);

						return result > 32;
					}
					else if (optype == "desktop")	//显示桌面
					{
						return helper::ShowDisktop();
					}
					else if (optype == "start") //显示开始菜单
					{
						return helper::show_StartMenu();
					}
					else if (optype == "run") //显示运行对话框
					{
						return helper::ShowRunDlg();
					}
					else if (optype == "msg") //弹出一条消息
					{
						etd::string_view param = item["param"];
						box.ShowInfo("{}", param);
						return true;
					}
					else if (optype == "null")	//啥也不作,只拦截此快捷键
					{
						return true;
					}else if (optype.empty())
					{
						box.ShowError("此快捷键缺少操作类型,如不需要任何操作只作拦截用,请使用\"null\"");
						return false;
					}
					else {
						box.ShowError("未识别该操作:{}",optype);
						return false;
					}
				});

			if (!result)
			{
				box.ShowError("注册快捷键失败:{}", key);
			}
		});

	return true;
}

bool easyWDM::SetHotkey(std::string hotkey, hotkey_handler&& handler)
{
	DWORD dwFlags = _KEYS_STATUS::keyString_to_Flags(hotkey);
	if (dwFlags == 0) return false;
	if (_map_hotkey.find(dwFlags) != _map_hotkey.end()) return false;
	_map_hotkey[dwFlags] = handler;
	return true;
}

bool easyWDM::initWDM()
{
	if (!initConfig()) return false;

	//窗口勾子
	if (_config["hook_windows"])
	{
		g_Shell_Wnd_Msg_ID = RegisterWindowMessageA("SHELLHOOK");
		if (g_Shell_Wnd_Msg_ID == 0)
		{
			box.ApiError("RegisterWindowMessageA", "SHELLHOOK");
			return false;
		}

		if (!RegisterShellHookWindow(_tray.GetWnd()))
		{
			box.ApiError("RegisterShellHookWindow");
			return false;
		}

		_tray.set_msg_handler(g_Shell_Wnd_Msg_ID, [&](WPARAM wParam, LPARAM lParam)
			{
				if (wParam == HSHELL_WINDOWCREATED)
				{
					WndHookProc((HWND)lParam, true);
				}
				else if (wParam == HSHELL_WINDOWACTIVATED
					|| wParam == HSHELL_RUDEAPPACTIVATED)
				{
					WndHookProc((HWND)lParam, false);
				}
			});
	}

	//键盘钩子
	if (_config["hook_key"])
	{
		_hookKey = SetWindowsHookExA(WH_KEYBOARD_LL, KeyHookProc, ::GetModuleHandleA(nullptr), NULL);
		if (!_hookKey)
		{
			UninstallHook();
			box.ApiError("SetWindowsHookExA", "WH_KEYBOARD_LL");
			return false;
		}
	}

	//鼠标钩子
	if (_config["hook_mouse"])
	{
		_hookMouse = SetWindowsHookExA(WH_MOUSE_LL, MouseHookProc, ::GetModuleHandleA(nullptr), NULL);
		if (!_hookMouse)
		{
			UninstallHook();
			box.ApiError("SetWindowsHookExA", "WH_MOUSE_LL");
			return false;
		}
	}

	return true;
}
