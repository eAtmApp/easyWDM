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
		process.exit("��ȡ�����ļ�ʧ��,���ʽ����,ɾ�������ļ����ɻָ�Ĭ��!");
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
	//ö����ʾ����Ϣ
	VERIFY(helper::enumMonitor(m_monitors) >= 1);

	m_monitors.forEach([](MONITOR_INFO& info)
		{
			auto& rct = info.rcMonitor;
			//console.log("��ʾ��: {:08X},{}*{} - {}*{}", (DWORD)info.hMonitor,rct.left,rct.top,rct.right,rct.bottom);
			console.log("��ʾ��:{:08X},{}", (DWORD)info.hMonitor, rct);
		});

}

bool easyWDM::set_limit_mouse()
{
	m_bIs_limit_mouse = !m_bIs_limit_mouse;
	console.debug("�������״̬:{}", m_bIs_limit_mouse);
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
					if (optype == "open")	//��Ӧ�û��ĵ�
					{
						etd::string_view path = item["path"];
						etd::string_view param = item["param"];
						etd::string_view dir = item["dir"];

						int showtype = -1;
						if (!item["show"].isNull()) showtype = item["show"].asInt();

						if (path.empty())
						{
							console.error("open����û��·��.");
							return false;
						}

						int errcode = 0;
						if (!helper::runApp(optype, path, param, dir, showtype, &errcode))
						{
							console.error("����{}ʧ��,���ش������:{}", path, errcode);
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
					else if (optype == "desktop")	//��ʾ����
					{
						return helper::ShowDisktop();
					}
					else if (optype == "start") //��ʾ��ʼ�˵�
					{
						return helper::show_StartMenu();
					}
					else if (optype == "run") //��ʾ���жԻ���
					{
						return helper::ShowRunDlg();
					}
					else if (optype == "msg") //����һ����Ϣ
					{
						etd::string_view param = item["param"];
						box.ShowInfo("{}", param);
						return true;
					}
					else if (optype == "null")	//ɶҲ����,ֻ���ش˿�ݼ�
					{
						return true;
					}
					else if (optype.empty())
					{
						box.ShowError("�˿�ݼ�ȱ�ٲ�������,�粻��Ҫ�κβ���ֻ��������,��ʹ��\"null\"");
						return false;
					}
					else {
						box.ShowError("δʶ��ò���:{}", optype);
						return false;
					}
				});

			if (!result)
			{
				box.ShowError("ע���ݼ�ʧ��:{}", key);
			}
		});

	//���ù���
	//SetHotkey("win+d", helper::ShowDisktop);
	//SetHotkey("win+r", std::bind(helper::ShowRunDlg, false));
	//SetHotkey("win", helper::show_StartMenu);

	//���̹���
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

	//��깳��
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

	//���ڹ���
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
		//0x06; //����   rid.usUsagePage = 0x01; rid.usUsage = 0x02; Ϊ���
		RAWINPUTDEVICE rid[2] = { 0 };  //�豸��Ϣ
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
					UINT uret = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, szBuffer, &uSize, sizeof(RAWINPUTHEADER)); //�Ȼ�ȡ���ݴ�СdwSize

					//if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) //��ȡ��Ϣ��Ϣ
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

						//console.log("�豸����:{}-{:08X} ����:{}", raw->header.dwType, (uint64_t)(raw->header.hDevice),
							//util::binary_to_hex({ (char*)raw + sizeof(RAWINPUTHEADER) + 8,(uint64_t)(raw->header.dwSize - sizeof(RAWINPUTHEADER) - 8) }));

						int x = 0;
						//console.log("��������:{}", );
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
				console.log("WM_WTSSESSION_CHANGE��Ϣ:{:02x}", wParam);
			});
	}
	else {
		console.error("WTSRegisterSessionNotificationʧ��,{}", ::GetLastError());
	}

	//ϵͳ���ñ仯ʱ��������Ϣ
	_tray.set_msg_handler(WM_SETTINGCHANGE, [&](WPARAM wParam, LPARAM lParam)
		{
			if (lParam != NULL)
			{
				eString str((char*)lParam);
				if (str.compare_icase("Environment"))
				{
					console.log("ˢ�½��̻�������");
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
