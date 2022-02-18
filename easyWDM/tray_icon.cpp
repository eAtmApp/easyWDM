#include "pch.h"
#include "tray_icon.h"
#include <stdexcept>
#include <easy/easy.h>
using namespace easy;

static constexpr auto WM_EASY_TRAY = WM_USER + 1000;

static constexpr auto wnd_class_name = "easy_trayicon_class";

//任务栏创建消息
UINT g_uTaskbarRestart;

tray_icon::tray_icon(int res_id, const char* tipText)
	:_hIcon(nullptr)
	, _tipText(tipText ? tipText : "")
{
	_atom = ::GlobalAddAtomA(wnd_class_name);
	if (!_atom)
	{
		box.ApiErrorExit("GlobalAddAtomA");
	}

	if (res_id)
	{
		SetIcon(res_id);
	}

	m_hMenu = ::CreatePopupMenu();
	if (!m_hMenu)
	{
		box.ApiErrorExit("CreatePopupMenu");
	}

	g_uTaskbarRestart = RegisterWindowMessageA("TaskbarCreated");
	if (!g_uTaskbarRestart)
	{
		box.ApiErrorExit("RegisterWindowMessageA");
	}

	memset(&_notify, 0, sizeof(NOTIFYICONDATA));

	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hInstance = ::GetModuleHandleA(nullptr);
	wcex.lpfnWndProc = tray_icon::WndProc;
	wcex.lpszClassName = wnd_class_name;

	if (RegisterClassEx(&wcex) == 0)
	{
		box.ApiErrorExit("RegisterClassEx");
	}

	m_hWnd = ::CreateWindowA(wnd_class_name, _tipText.c_str(), 0, 0, 0, 0, 0, nullptr, nullptr, wcex.hInstance, nullptr);
	if (!m_hWnd)
	{
		box.ApiErrorExit("CreateWindowA");
		return;
	}

	UpdateWindow(m_hWnd);

	::SetPropA(m_hWnd, wnd_class_name, (HANDLE)this);
	_UpdateTrayIcon();
}

tray_icon::~tray_icon()
{
	DeleteTray();

	if (m_hMenu)
	{
		DestroyMenu(m_hMenu);
	}
}


void tray_icon::SetIcon(int resid)
{
	_hIcon = LoadIcon(::GetModuleHandleA(nullptr), MAKEINTRESOURCEA(resid));
	if (_notify.hWnd) _UpdateTrayIcon(false);
}

void tray_icon::DeleteTray()
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
	_cm_msg_union msgunion;
	msgunion.cb = std::move(handler);
	_map_msg_handler[msg] = std::move(msgunion);
}
void tray_icon::set_msg_handler(UINT msg, cb_wm_msg_np&& handler)
{
	_cm_msg_union msgunion;
	msgunion.cb_mp = std::move(handler);
	_map_msg_handler[msg] = std::move(msgunion);
}

void tray_icon::set_msg_handler(UINT msg)
{
	_map_msg_handler.erase(msg);
}

bool tray_icon::GetCheck(uint32_t id)
{
	MENUITEMINFO info;
	info.cbSize = sizeof(MENUITEMINFO); // must fill up this field
	info.fMask = MIIM_STATE;            // get the state of the menu item
	VERIFY(::GetMenuItemInfoA(m_hMenu, id, false, &info));

	return info.fState & MF_CHECKED;
}

void tray_icon::SetCheck(uint32_t id, bool is_checked)
{
	VERIFY(::CheckMenuItem(m_hMenu, id, MF_BYCOMMAND | (is_checked ? MF_CHECKED : MF_UNCHECKED)) != -1);
}

bool tray_icon::RegisterHotkey(UINT fsModifiers, UINT vk, cb_hotkey&& cb)
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
		else {
			show_err("热键被占用");
		}
		return false;
	}
}

bool tray_icon::UnregisterHotkey(UINT fsModifiers, UINT vk)
{
	auto id = MAKELONG(fsModifiers, vk);
	return ::UnregisterHotKey(m_hWnd, id);
}

void tray_icon::PopupMenu()
{
	POINT p;
	GetCursorPos(&p);
	SetForegroundWindow(m_hWnd);
	_last_cmd = ::TrackPopupMenu(m_hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_BOTTOMALIGN, p.x, p.y, 0, m_hWnd, nullptr);
	if (_last_cmd == 0) return;

	auto cur = _map_menu_handler.find(_last_cmd);

	if (cur != _map_menu_handler.end())
	{
		cur->second();
	}
	else {
		if (_cb_items_click)
		{
			_cb_items_click(_last_cmd);
		}
	}
}

void tray_icon::_UpdateTrayIcon(bool isReset)
{
	if (isReset) DeleteTray();

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
	_notify.uFlags = NIF_ICON | NIF_MESSAGE | NIF_INFO;
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
		case WM_DESTROY:
		{
			::RemovePropA(hWnd, wnd_class_name);
			::PostQuitMessage(0);
			break;
		}
		default:
		{
			auto pThis = (tray_icon*)::GetPropA(hWnd, wnd_class_name);
			if (pThis && pThis->UserMessage(message, wParam, lParam))
			{
				return true;
			}
		}
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	bool tray_icon::UserMessage(UINT message, WPARAM wParam, LPARAM lParam)
	{

		//托盘消息
		if (message == WM_EASY_TRAY && lParam == WM_RBUTTONUP)	//托盘右键菜单
		{
			PopupMenu();
			return true;
		}
		else if (message == WM_HOTKEY)
		{
			auto item = _map_hotkey_handler.find(wParam);
			if (item != _map_hotkey_handler.end())
			{
				item->second();
				return true;
			}
			return true;
		}
		else if (message == g_uTaskbarRestart)
		{
			_UpdateTrayIcon(true);
			return true;
		}

		//处理自定义消息
		if (!_map_msg_handler.empty())
		{
			auto it = _map_msg_handler.find(message);
			if (it != _map_msg_handler.end())
			{
				it->second(wParam, lParam);
				if (message != WM_INPUT) return true;
				else return false;
			}
		}

		return false;
	}
	
	void tray_icon::run()
	{
		MSG msg;
		while (GetMessageA(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
