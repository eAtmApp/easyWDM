#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>
#include <map>

#include "E:\\easy_cpp\\easylib\\easylib\\include\\mfc_macro.h"

class tray_icon;

typedef std::function<void()> cb_item_click;

typedef cb_item_click cb_hotkey;

typedef std::function<void(uint32_t)> cb_menu_click;

typedef std::function<void(WPARAM wParam, LPARAM lParam)> cb_wm_msg;
typedef std::function<void()>  cb_wm_msg_np;

class tray_icon
{
public:
	tray_icon()
		:tray_icon(0, nullptr)
	{
	}
	tray_icon(const char* tipText)
		:tray_icon(0, tipText)
	{
	}
	tray_icon(int res_id)
		:tray_icon(res_id, nullptr)
	{
	}
	tray_icon(int res_id, const char* tipText);
	virtual ~tray_icon();

	void	show_err(const char* str);
	void	show_info(const char* str);

	void run();

	void SetIcon(int resid);

	void DeleteTray();

	void	close();

	HWND	GetWnd()
	{
		return m_hWnd;
	}
	
	//设置消息处理程序
	void	set_msg_handler(UINT msg, cb_wm_msg &&handler);
	void	set_msg_handler(UINT msg, cb_wm_msg_np&& handler);
	void	set_msg_handler(UINT msg);	//无参则删除处理程序
public:
	void	set_menu_handler(cb_menu_click&& handler)
	{
		_cb_items_click = std::move(handler);
	}

	//菜单选中
	bool	GetCheck()
	{
		return GetCheck(_last_cmd);
	}
	bool	GetCheck(uint32_t id);
	void	SetCheck(bool is_checked)
	{
		return SetCheck(_last_cmd, is_checked);
	}
	void	SetCheck(uint32_t id, bool is_checked);
	
	//添加热键
	bool		RegisterHotkey(UINT fsModifiers, UINT vk, cb_hotkey&& cb);
	bool		UnregisterHotkey(UINT fsModifiers, UINT vk);

	uint32_t	AddMenu(const char* title, uint32_t id = 0, bool is_enabled = true)
	{
		return _AppendMenu(title, id, is_enabled);
	}
	uint32_t	AddMenu(const char* title, uint32_t id, cb_item_click&& handler, bool is_enabled = true)
	{
		auto ret = _AppendMenu(title, id, is_enabled);
		if(ret) _map_menu_handler[ret] = handler;
		return ret;
	}
	uint32_t	AddMenu(const char* title, cb_item_click&& handler, uint32_t id=0, bool is_enabled = true)
	{
		auto ret = _AppendMenu(title, id, is_enabled);
		if (ret) _map_menu_handler[ret] = handler;
		return ret;
	}
	void		AddSeparator()
	{
		::AppendMenuA(m_hMenu, MF_SEPARATOR, 0, nullptr);
	}

	//弹出菜单
	void PopupMenu();

	uint32_t _AppendMenu(const char* title, uint32_t id, bool is_enabled)
	{
		if (id == 0) id = ++g_menu_id;
		auto result = ::AppendMenuA(m_hMenu, is_enabled ? MF_ENABLED : MF_GRAYED, id, title);
		ASSERT(result);
		return id;
	}

private:
	//更新图标-是否重置
	void	_UpdateTrayIcon(bool isReset = false);
private:

	uint32_t	_last_cmd = 0;

	std::map<uint32_t, cb_item_click> _map_menu_handler;
	inline static uint32_t g_menu_id = 1000;
	HMENU	m_hMenu = nullptr;
	cb_menu_click	_cb_items_click = nullptr;

	std::map<WPARAM, cb_item_click> _map_hotkey_handler;
	
	struct _cm_msg_union
	{
		cb_wm_msg_np cb_mp = nullptr;
		cb_wm_msg	cb = nullptr;
		void operator()(WPARAM wParam, LPARAM lParam) const
		{
			if (cb_mp) cb_mp();
			else cb(wParam, lParam);
		}
	};
	std::map<UINT, _cm_msg_union> _map_msg_handler;

	std::string _tipText;

	HWND m_hWnd = nullptr;
	
	HICON	_hIcon = nullptr;

	NOTIFYICONDATA _notify;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};