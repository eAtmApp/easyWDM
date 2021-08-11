#include "tray_icon.h"
#include <stdexcept>

static constexpr auto WM_EASY_TRAY = WM_USER + 1000;

static constexpr auto wnd_class_name = "easy_trayicon_class";

UINT g_uTaskbarRestart;

tray_icon::tray_icon(int res_id, const char* tipText)
	:_hIcon(nullptr)
	, _tipText(tipText ? tipText : "")
{
	if (res_id)
	{
		set_icon(res_id);
	}
	
	m_hMenu = ::CreatePopupMenu();
	if (!m_hMenu)
	{
		throw std::runtime_error("CreatePopupMenu Failed");
	}

	g_uTaskbarRestart = RegisterWindowMessageA("TaskbarCreated");
	if (!g_uTaskbarRestart)
	{
		throw std::runtime_error("RegisterWindowMessageA(TaskbarCreated)  Failed");
	}

	memset(&_notify, 0, sizeof(NOTIFYICONDATA));

	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hInstance = ::GetModuleHandleA(nullptr);
	wcex.lpfnWndProc = tray_icon::WndProc;
	wcex.lpszClassName = wnd_class_name;

	if (RegisterClassEx(&wcex) == 0)
	{
		throw std::runtime_error("RegisterClassEx Failed");
	}

	m_hWnd = ::CreateWindowA(wnd_class_name, _tipText.c_str(), 0, 0, 0, 0, 0, nullptr, nullptr, wcex.hInstance, nullptr);
	if (!m_hWnd)
	{
		throw std::runtime_error("CreateWindow Failed");
	}
	
	UpdateWindow(m_hWnd);
}

tray_icon::~tray_icon()
{
	delete_tray();

	if (m_hMenu)
	{
		DestroyMenu(m_hMenu);
	}
}

void tray_icon::run()
{
	update_trayIcon();

	MSG msg;
	while (GetMessageA(&msg, nullptr, 0, 0))
	{
		if (m_hWnd == msg.hwnd)
		{
			switch (msg.message)
			{
			case WM_EASY_TRAY:
			case WM_EASY_TRAY + 1:		//托盘消息
			{
				if (msg.lParam == WM_RBUTTONUP)
				{
					POINT p;
					GetCursorPos(&p);
					SetForegroundWindow(msg.hwnd);
					PopupMenu();
				}
				continue;
			}
			case WM_HOTKEY:			//处理自定义热键消息
			{
				auto item = _map_hotkey_handler.find(msg.wParam);
				if (item != _map_hotkey_handler.end())
				{
					item->second();
				}
				continue;
			}
			case WM_DESTROY:		//窗口被关闭
				break;
			case SPI_SETICONTITLEWRAP:	//资源管理器重启
				int x = 0;
				break;
			}

			//处理自定义消息
			if (!_map_msg_handler.empty())
			{
				auto it = _map_msg_handler.find(msg.message);
				if (it != _map_msg_handler.end())
				{
					it->second(msg.wParam, msg.lParam);
					continue;
				}
			}

			//处理重建托盘
			if (g_uTaskbarRestart == msg.message)
			{
				update_trayIcon(true);
				continue;
			}
		}



		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void tray_icon::set_icon(int resid)
{
	_hIcon = LoadIcon(::GetModuleHandleA(nullptr), MAKEINTRESOURCEA(resid));
	if (_notify.hWnd) update_trayIcon(false);
}

void tray_icon::delete_tray()
{
	if (_notify.hWnd)
	{
		Shell_NotifyIcon(NIM_DELETE, &_notify);
		memset(&_notify, 0, sizeof(NOTIFYICONDATA));
	}
}

void tray_icon::close()
{
	::PostMessageA(m_hWnd, WM_CLOSE, 0, 0);
}

void tray_icon::set_msg_handler(UINT msg, cb_wm_msg&& handler)
{
	_map_msg_handler[msg] = std::move(handler);
}

bool tray_icon::is_menu_checked(uint32_t id)
{
	MENUITEMINFO info;
	info.cbSize = sizeof(MENUITEMINFO); // must fill up this field
	info.fMask = MIIM_STATE;            // get the state of the menu item
	VERIFY(::GetMenuItemInfoA(m_hMenu, id, false, &info));

	return info.fState & MF_CHECKED;
}

void tray_icon::set_menu_checked(uint32_t id, bool is_checked)
{
	ASSERT(::CheckMenuItem(m_hMenu, id, MF_BYCOMMAND | (is_checked ? MF_CHECKED : MF_UNCHECKED)) != -1);
	//MENUITEMINFO info;
	//info.cbSize = sizeof(MENUITEMINFO);
	//::SetMenuItemInfoA(m_hMenu, id, false, &info);
}

bool tray_icon::register_hotkey(UINT fsModifiers, UINT vk, cb_hotkey&& cb)
{
	ASSERT(m_hWnd);
	auto id = MAKELONG(fsModifiers, vk);
	if (RegisterHotKey(m_hWnd, id, fsModifiers, vk))
	{
		_map_hotkey_handler[id] = cb;
		return true;
	}
	else {
		auto err = ::GetLastError();
		if (err != ERROR_HOTKEY_ALREADY_REGISTERED)
		{
			throw std::runtime_error("RegisterHotKey Failed");
		}
		return false;
	}
}

void tray_icon::unregister_hotkey(UINT fsModifiers, UINT vk)
{
	auto id = MAKELONG(fsModifiers, vk);
	ASSERT(::UnregisterHotKey(m_hWnd, id));
}

void tray_icon::PopupMenu()
{
	POINT p;
	GetCursorPos(&p);
	SetForegroundWindow(m_hWnd);
	uint32_t cmd = ::TrackPopupMenu(m_hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_BOTTOMALIGN, p.x, p.y, 0, m_hWnd, nullptr);
	if (cmd == 0) return;

	auto cur = _map_menu_handler.find(cmd);
	if (cur != _map_menu_handler.end())
	{
		cur->second();
	}
	else {
		if (_cb_items_click) _cb_items_click(cmd);
	}
}

void tray_icon::update_trayIcon(bool isReset)
{
	if (isReset) delete_tray();

	auto opType = NIM_ADD;
	if (_notify.hWnd)
	{
		opType = NIM_MODIFY;
	}

	_notify.cbSize = sizeof(NOTIFYICONDATA);
	_notify.hWnd = m_hWnd;
	_notify.uFlags = NIF_ICON | NIF_MESSAGE;
	_notify.uCallbackMessage = WM_EASY_TRAY;
	_notify.hIcon = _hIcon;

	if (!_tipText.empty())
	{
		strcpy_s(_notify.szTip, sizeof(_notify.szTip), _tipText.c_str());
		_notify.uFlags |= NIF_TIP;
	}

	if (!Shell_NotifyIcon(opType, &_notify))
	{
		throw std::runtime_error("Shell_NotifyIcon Failed");
	}
}

void tray_icon::show_err(const char* str)
{
	_notify.uFlags = NIF_ICON | NIF_MESSAGE| NIF_INFO;
	strcpy_s(_notify.szInfo, sizeof(_notify.szInfo), str);
	_notify.uTimeout = 10000;
	_notify.dwInfoFlags = NIIF_ERROR;
	Shell_NotifyIcon(NIM_MODIFY, &_notify);
}

void tray_icon::show_info(const char* str)
{
	_notify.uFlags = NIF_ICON | NIF_MESSAGE | NIF_INFO;
	strcpy_s(_notify.szInfo, sizeof(_notify.szInfo), str);
	_notify.uTimeout = 10000;
	Shell_NotifyIcon(NIM_MODIFY, &_notify);
}

LRESULT CALLBACK tray_icon::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_EASY_TRAY:		//托盘消息
	{
		if (lParam == WM_RBUTTONUP)
		{
			PostMessageA(hWnd, WM_EASY_TRAY + 1, wParam, lParam);
		}
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		if (message == g_uTaskbarRestart)
		{
			PostMessageA(hWnd, g_uTaskbarRestart, wParam, lParam);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}