#include "pch.h"
#include "easyWDM.h"

void easyWDM::WndHookProc(HWND hWnd, bool isCreate)
{
	if (!hWnd) return;

	auto hMonitor = helper::getCurrentMonitor();

	helper::showOwerWnd(hMonitor,false);

	//调试
	{
		auto txtName = helper::getWndTitle(hWnd);
		auto className = helper::getWndClass(hWnd);
		//console.log("激活窗口:{} - {}", className, txtName);
	}
	
	//只转移创建后5秒内显示的窗口
	constexpr auto MAX_WAIT_TIME = 5;

	static auto s_last_clear_tick = (DWORD)(::GetTickCount64() / 1000);

	auto dwCurTick = (DWORD)(::GetTickCount64() / 1000);

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

	auto wndMonitor = MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST);
	if (wndMonitor == hMonitor || wndMonitor == nullptr) return;

	RECT monRct = { 0 };
	if (!helper::getCurrentMonitorRecv(&monRct)) return;

	//这里要计算位置

	int wndWidth = rct.right - rct.left;
	int wndHeight = rct.bottom - rct.top;

	int monWidth = monRct.right - monRct.left;
	int monHeight = monRct.bottom - monRct.top;

	bool is_handler = false;

	int x = 0, y = 0;

	//处理运行窗口
	/*
	if (wndWidth < 500 && wndWidth >= 400 && wndHeight <= 300 && wndHeight >= 200
		&& helper::getWndTitle(hWnd) == "运行" && helper::getWndClass(hWnd) == "#32770")
	{
		x = monRct.left;
		y = monRct.bottom - wndHeight;

		HWND hTaskBar = helper::GetCurrentMonitorStartMenuWnd();
		hTaskBar = ::GetParent(hTaskBar);
		if (hTaskBar)
		{
			RECT rct = { 0 };
			if (::GetWindowRect(hTaskBar, &rct))
			{
				auto barWidth = rct.right - rct.left;
				auto barHeight = rct.bottom - rct.top;
				if (rct.left = monRct.left)
				{
					//在下面
					if (barWidth == monWidth && rct.bottom == monRct.bottom)
					{
						y -= barHeight;
					}
					else if (barHeight == monHeight)
					{
						x += barWidth;
					}
				}
				is_handler = true;
			}
		}
	}*/

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

	auto ret = SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);

	console.log("修改位置-相对坐标:{}*{}", (int)(x - monRct.left), (int)(y - monRct.top));
}
