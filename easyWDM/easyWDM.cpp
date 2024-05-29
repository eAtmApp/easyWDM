#include "pch.h"
#include "easyWDM.h"
#include "Resource.h"
#include <hidsdi.h>
#pragma comment(lib, "Dwmapi.lib")

#include <wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")



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
		_hookMouse = nullptr;
	}
}

bool easyWDM::initConfig()
{
	_config["raw_input"] = false;
	_config["hook_key"] = true;
	_config["hook_mouse"] = true;
	_config["hook_windows"] = true;
	_config["hotkey"] = {};
	auto& win = _config["hotkey"];
	win["win"] = {
			{"type","start"},
	};
	win["win+d"] = {
			{"type","desktop"},
	};
	win["win+r"] = {
			{"type","run"},
	};
	win["win+c"] = {
			{"type","open"},
			{"path","cmd.exe"},
			{"dir","d:\\"}
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

	if (!_config.readConfig())
	{
		process.exit("读取配置文件失败,如格式错乱,删除配置文件即可恢复默认!");
		return false;
	}

	return true;
}

bool easyWDM::SetHotkey(std::string hotkey, hotkey_handler&& handler)
{
	DWORD dwFlags = _KEYS_STATUS::keyString_to_Flags(hotkey);
	if (dwFlags == 0) return false;
	if (handler)
	{
		if (_map_hotkey.find(dwFlags) != _map_hotkey.end()) return false;
		_map_hotkey[dwFlags] = handler;
	}
	else {
		_map_hotkey.erase(dwFlags);
	}
	return true;
}

HMONITOR easyWDM::getMonitor(POINT& pt)
{
	HMONITOR result = nullptr;
	m_monitors.some([&](HMONITOR hMonitor, MONITOR_INFO& info)
		{
			auto& rct = info.rcMonitor;
			if (pt.x >= rct.left && pt.x < rct.right
				&& pt.y >= rct.top && pt.y < rct.bottom)
			{
				result = hMonitor;
				return true;
			}
			return false;
		});
	return result;
}

void easyWDM::refreshMonitor()
{
	//枚举显示器信息
	VERIFY(helper::enumMonitor(m_monitors) >= 1);

	m_monitors.forEach([](MONITOR_INFO& info)
		{
			auto& rct = info.rcMonitor;
			//console.log("显示器: {:08X},{}*{} - {}*{}", (DWORD)info.hMonitor,rct.left,rct.top,rct.right,rct.bottom);
			console.log("显示器:{:08X},{}", (DWORD)info.hMonitor, rct);
		});

}

bool easyWDM::set_limit_mouse()
{
	m_bIs_limit_mouse = !m_bIs_limit_mouse;
	console.debug("鼠标限制状态:{}", m_bIs_limit_mouse);
	if (m_bIs_limit_mouse)
	{
		_tray.SetIcon(IDI_LOCK);
	}
	else {
		_tray.SetIcon(IDI_TRAYICONDEMO);
	}

	_config["m_bIs_limit_mouse"] = m_bIs_limit_mouse;
	_config.writeConfig();

	return true;
}

bool easyWDM::initWDM()
{
	//CoInitialize(NULL);
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	auto wnd = helper::GetCurrentMonitorStartMenuWnd();

	//helper::UIAutoInvoke(wnd);

	::Sleep(1000);

	refreshMonitor();

	if (!initConfig()) return false;

	if (_config["hook_mouse"])
	{
		SetHotkey("ctrl", std::bind(&easyWDM::set_limit_mouse, this));
		_tray.SetIcon(IDI_LOCK);
	}

	jsoncpp& hotkey = _config["hotkey"];
	hotkey.forEach([&](std::string_view key, jsoncpp& item)
		{
			bool result = SetHotkey(std::string(key), [this, &item]()
				{
					etd::string optype = item["type"];
					optype.tolower();
					if (optype == "open")	//打开应用或文档
					{
						etd::string_view path = item["path"];
						etd::string_view param = item["param"];
						etd::string_view dir = item["dir"];

						int showtype = -1;
						if (!item["show"].isNull()) showtype = item["show"].asInt();

						if (path.empty())
						{
							console.error("open操作没有路径.");
							return false;
						}

						int errcode = 0;
						if (!helper::runApp(optype, path, param, dir, showtype, &errcode))
						{
							console.error("运行{}失败,返回错误代码:{}", path, errcode);
							return false;
						}

						return true;

						/*
												auto result = ShellExecuteA(nullptr,
													optype.empty() ? nullptr : optype.data(),
													path.empty() ? nullptr : path.data(),
													param.empty() ? nullptr : param.data(),
													dir.empty() ? nullptr : dir.data(),
													SW_SHOWNORMAL);

												return result > (HINSTANCE)32;*/
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
					}
					else if (optype.empty())
					{
						box.ShowError("此快捷键缺少操作类型,如不需要任何操作只作拦截用,请使用\"null\"");
						return false;
					}
					else {
						box.ShowError("未识别该操作:{}", optype);
						return false;
					}
				});

			if (!result)
			{
				box.ShowError("注册快捷键失败:{}", key);
			}
		});

	//内置功能
	//SetHotkey("win+d", helper::ShowDisktop);
	//SetHotkey("win+r", std::bind(helper::ShowRunDlg, false));
	//SetHotkey("win", helper::show_StartMenu);

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

	m_bIs_limit_mouse = _config["m_bIs_limit_mouse"];
	if (m_bIs_limit_mouse)
	{
		_tray.SetIcon(IDI_LOCK);
	}
	else {
		_tray.SetIcon(IDI_TRAYICONDEMO);
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

	//窗口勾子
	if (_config["hook_windows"])// || _config["hook_key"])
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
				bool is_create = false;

				auto hWnd = (HWND)lParam;

				if (wParam == HSHELL_WINDOWCREATED
					|| wParam == HSHELL_WINDOWACTIVATED
					|| wParam == HSHELL_RUDEAPPACTIVATED)
				{
					bool isCreate = wParam == HSHELL_WINDOWCREATED;
					std::thread thread_(&easyWDM::WndHookProc, this, hWnd, isCreate);
					thread_.detach();
				}
			});
	}

	if (_config["raw_input"])
	{
		//0x06; //键盘   rid.usUsagePage = 0x01; rid.usUsage = 0x02; 为鼠标
		RAWINPUTDEVICE rid[2] = { 0 };  //设备信息
		rid[0].usUsagePage = 0xFF00;
		rid[0].usUsage = 0;
		rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_PAGEONLY;
		rid[0].hwndTarget = _tray.GetWnd();

		if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE)))
		{
			UninstallHook();
			box.ApiError("RegisterRawInputDevices");
			return false;
		}

		_tray.set_msg_handler(WM_INPUT, [&](WPARAM wParam, LPARAM lParam)
			{
				char szBuffer[1024] = { 0 };
				do
				{
					UINT uSize = sizeof(szBuffer);
					UINT uret = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, szBuffer, &uSize, sizeof(RAWINPUTHEADER)); //先获取数据大小dwSize

					//if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) //获取消息信息
						//break;

					RAWINPUT* raw = (RAWINPUT*)szBuffer;
					//if (raw->header.dwType!= RIM_TYPEMOUSE) break;
					//if (raw->header.dwType == RIM_TYPEMOUSE) break;

					if (raw->header.dwType == RIM_TYPEHID)
					{
						char szName[2048] = { 0 };
						if (HidD_GetProductString(raw->header.hDevice, szName, sizeof(szName)))
						{
							wchar_t* cao = (wchar_t*)szName;
							auto astr = util::unicode_ansi(cao);
							console.log("{}\n", astr);
						}

						char szDevName[1024] = { 0 };
						UINT uSize = sizeof(szDevName);
						auto resize = GetRawInputDeviceInfoA(raw->header.hDevice, RIDI_DEVICENAME, szDevName, &uSize);

						auto hid = raw->data.hid;

						//console.log("设备类型:{}-{:08X} 数据:{}", raw->header.dwType, (uint64_t)(raw->header.hDevice),
							//util::binary_to_hex({ (char*)raw + sizeof(RAWINPUTHEADER) + 8,(uint64_t)(raw->header.dwSize - sizeof(RAWINPUTHEADER) - 8) }));

						int x = 0;
						//console.log("读到数据:{}", );
					}

					auto& mouse = raw->data.mouse;
					//mouse.usButtonData

					INT64* pInt64 = (INT64*)&mouse;
					//console.log("{:08X} {:08X} {:08X} {:08X} {:08X}", pInt64[0], pInt64[1], pInt64[2], pInt64[3], pInt64[4]);

					break;

				} while (false);
			});
	}

	if (WTSRegisterSessionNotification(_tray.GetWnd(), NOTIFY_FOR_THIS_SESSION))
	{
		_tray.set_msg_handler(WM_WTSSESSION_CHANGE, [&](WPARAM wParam, LPARAM lParam)
			{
				if (wParam == WTS_SESSION_UNLOCK)
				{
					key_status.reset();
				}
				m_filter_win_status = false;
				console.log("WM_WTSSESSION_CHANGE消息:{:02x}", wParam);
			});
	}
	else {
		console.error("WTSRegisterSessionNotification失败,{}", ::GetLastError());
	}

	//系统设置变化时触发此消息
	_tray.set_msg_handler(WM_SETTINGCHANGE, [&](WPARAM wParam, LPARAM lParam)
		{
			if (lParam != NULL)
			{
				eString str((char*)lParam);
				if (str.compare_icase("Environment"))
				{
					console.log("刷新进程环境变量");
					process.reload_env();
				}
			}
		});

	//init_hid();

	//std::thread rawinput(&easyWDM::initRawInput, this);
	//rawinput.detach();

	_BluetoothMac = _config["Bluetooth"].asString();
	if (!_BluetoothMac.empty())
	{
		worker.setInterval([this]()
			{
				bluetooth_check();
			}, 1000);
	}
	return true;
}
