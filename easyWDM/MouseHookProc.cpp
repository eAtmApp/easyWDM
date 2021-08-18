#include "pch.h"
#include "easyWDM.h"

bool easyWDM::MouseMessage(UINT button, POINT pt)
{
	//当前限制区域
	static RECT s_limit_rect = { 0 };

	//是否设置限制区域
	static bool s_is_set_limit_rect = false;

	static bool s_lbutton_is_down = false;

	static HWND s_wnd_lock = nullptr;

	//处理鼠标中键-Alt+中键 记住窗口,Ctrl+中键 恢复记住窗口
/*
	if (button == EWM_MBUTTONDOWN)
	{
		if (is_DownKey(VK_MENU))
		{
			s_wnd_lock = _GetCursorTopWnd();
			//console.log("CLASS:{:08X}-{}", (DWORD)tmp, getWndClass(tmp));
			//ShowWindow(s_wnd_lock, SW_MINIMIZE);
		}
		else if (is_DownKey(VK_CONTROL) && s_wnd_lock)
		{
			auto clsname = getWndClass(_GetCursorWnd());

			console.log("CLASS: {}", clsname);

			//排队在任务栏窗口列表中按中键
			if (clsname != "MSTaskListWClass" && clsname != "MSTaskSwWClass")
			{
				if (IsIconic(s_wnd_lock))
				{
					ShowWindow(s_wnd_lock, SW_SHOWNOACTIVATE);
				}
				activeWnd(s_wnd_lock);
				return true;
			}

		}

	}
	if (button == EWM_LBUTTONDOWN) s_lbutton_is_down = true;
	else if (button == EWM_LBUTTONUP) s_lbutton_is_down = false;
	//按住左键,单击右键-临时开关鼠标限制
	if (g_enable_mouse_limit && button == EWM_RBUTTONDOWN && s_lbutton_is_down)
	{
		g_temp_close_limit = !g_temp_close_limit;
		console.log("临时{}鼠标限制", g_temp_close_limit);
		if (g_temp_close_limit) ::ClipCursor(nullptr);
		return true;
	}*/

	if (!m_bIs_limit_mouse)
	{
		if (s_is_set_limit_rect)
		{
			console.debug("解除限制区域");
			::ClipCursor(nullptr);
			s_is_set_limit_rect = false;
		}
	}
	else {
		if (!s_is_set_limit_rect)
		{
			console.debug("设置限制区域");
			if (helper::getCurrentMonitorRecv(&s_limit_rect))
			{
				s_is_set_limit_rect = true;
			}
			else {
				console.error("获取当前监视器区域失败");
				return false;
			}
		}

		//全部取出到栈
		LONG x = pt.x;
		LONG y = pt.y;
		if (x < s_limit_rect.left+3) x = s_limit_rect.left;
		else if (x > s_limit_rect.right-3) x = s_limit_rect.right;
		if (y < s_limit_rect.top+3) y = s_limit_rect.top;
		else if (y > s_limit_rect.bottom-3) y = s_limit_rect.bottom;

		//被限制了.
		if (x != pt.x || y != pt.y)
		{
			::ClipCursor(&s_limit_rect);
		}
	}

	return false;
}

LRESULT CALLBACK easyWDM::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	//console.time();
	LRESULT lresult = false;
	if (nCode < 0)
	{
		lresult= CallNextHookEx(_hookMouse, nCode, wParam, lParam);
	} else if (nCode == HC_ACTION && m_pThis->MouseMessage((UINT)wParam, ((MOUSEHOOKSTRUCT*)lParam)->pt))
	{
		lresult= true;
	}
	else {
		lresult= CallNextHookEx(_hookKey, nCode, wParam, lParam);
	}
	//console.timeEnd();
	return lresult;
}