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

	void set_icon(int resid);

	void delete_tray();

	void	close();

	HWND	get_hwnd()
	{
		return m_hWnd;
	}
	
	//设置消息处理程序
	void	set_msg_handler(UINT msg, cb_wm_msg &&handler);
public:
	void	set_menu_handler(cb_menu_click&& handler)
	{
		_cb_items_click = std::move(handler);
	}

	//菜单选中
	bool	is_menu_checked(uint32_t id);
	void	set_menu_checked(uint32_t id, bool is_checked);
	
	//添加热键
	bool		register_hotkey(UINT fsModifiers, UINT vk, cb_hotkey&& cb);
	void		unregister_hotkey(UINT fsModifiers, UINT vk);

	uint32_t	add_menu(const char* title, uint32_t id = 0, bool is_enabled = true)
	{
		return _AppendMenu(title, id, is_enabled);
	}
	uint32_t	add_menu(const char* title, uint32_t id, cb_item_click&& handler, bool is_enabled = true)
	{
		auto ret = _AppendMenu(title, id, is_enabled);
		if(ret) _map_menu_handler[ret] = handler;
		return ret;
	}
	uint32_t	add_menu(const char* title, cb_item_click&& handler, uint32_t id=0, bool is_enabled = true)
	{
		auto ret = _AppendMenu(title, id, is_enabled);
		if (ret) _map_menu_handler[ret] = handler;
		return ret;
	}
	void		add_separator()
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
	void	update_trayIcon(bool isReset = false);
private:

	std::map<uint32_t, cb_item_click> _map_menu_handler;
	inline static uint32_t g_menu_id = 1000;
	HMENU	m_hMenu = nullptr;
	cb_menu_click	_cb_items_click = nullptr;

	std::map<WPARAM, cb_item_click> _map_hotkey_handler;

	std::map<UINT, cb_wm_msg> _map_msg_handler;

	std::string _tipText;

	HWND m_hWnd = nullptr;
	
	HICON	_hIcon = nullptr;

	NOTIFYICONDATA _notify;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};