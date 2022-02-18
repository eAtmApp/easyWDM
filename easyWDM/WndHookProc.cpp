#include "pch.h"
#include "easyWDM.h"

void easyWDM::WndHookProc(HWND hWnd, bool isCreate)
{
	if (!hWnd) return;
		
	auto hCurMonitor = helper::getCurrentMonitor();

	helper::showOwerWnd(hCurMonitor, false);
	


	//调试
	auto txtName = helper::getWndTitle(hWnd);
	auto className = helper::getWndClass(hWnd);
	//console.log("激活窗口:{} - {}", className, txtName);
	
	//只转移创建后5秒内显示的窗口
	constexpr auto MAX_WAIT_TIME = 5;

	static auto s_last_clear_tick = (DWORD)(::GetTickCount64() / 1000);

	auto dwCurTick = (DWORD)(::GetTickCount64() / 1000);

	EASY_STATIC_LOCK();

	if (isCreate)
	{
		_mapWndTick[hWnd] = dwCurTick;
	}
	else {
		auto it = _mapWndTick.find(hWnd);
		if (it == _mapWndTick.end()) return;

		DWORD dwCreateTick = it->second;
		_mapWndTick.erase(hWnd);

		auto dwTick = (DWORD)(::GetTickCount64() / 1000);
		if (dwCurTick - dwCreateTick > MAX_WAIT_TIME) return;
	}

	//每隔MAX_WAIT_TIME*2 清理一次map
	if (dwCurTick - s_last_clear_tick > MAX_WAIT_TIME * 2)
	{
		for (auto it = _mapWndTick.begin(); it != _mapWndTick.end();)
		{
			auto itbak = it;
			auto key = it->first;
			auto val = it->second;
			++it;
			if (dwCurTick - val > MAX_WAIT_TIME)
			{
				_mapWndTick.erase(itbak);
			}
		}
	}

	RECT rct = { 0 };
	if (!::GetWindowRect(hWnd, &rct)) return;

	auto wndMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (wndMonitor == hCurMonitor) return;

	if (wndMonitor == nullptr)
	{
		console.error("得到窗口显示器句柄失败:{}", className);
		return;
	}

	RECT monRct = { 0 };
	if (!helper::getCurrentMonitorRecv(&monRct)) return;

	//这里要计算位置

	int wndWidth = rct.right - rct.left;
	int wndHeight = rct.bottom - rct.top;

	int monWidth = monRct.right - monRct.left;
	int monHeight = monRct.bottom - monRct.top;

	bool is_handler = false;

	int x = 0, y = 0;
	
	if (!is_handler)
	{
		POINT pos;
		if (!GetCursorPos(&pos)) return;

		if (wndWidth >= monWidth) x = monRct.left;
		else {
			x = pos.x - wndWidth / 2;
			if (x + wndWidth > monRct.right) x = monRct.right - wndWidth;
			if (x < monRct.left) x = monRct.left;
		}
		if (wndHeight >= monHeight) y = monRct.top;
		else {
			y = pos.y - wndHeight / 2;
			if (y + wndHeight > monRct.bottom) y = monRct.bottom - wndHeight;
			if (y < monRct.top) y = monRct.top;
		}
	}

	//::MoveWindow(hWnd, x, y, wndWidth, wndHeight, TRUE);

	//auto ret = SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
	auto ret = SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_DRAWFRAME);
	
	console.log("转移显示器({}): {}*{}", className,(int)(x - monRct.left), (int)(y - monRct.top));
}