#include "pch.h"
#include "easyWDM.h"

//�����Ƿ�ƥ��
bool easyWDM::is_match(eString configName, eStringV psName, eStringV title, eStringV cls)
{
	if (psName.empty()) return false;

	//hook_windows_filter

	if (_config[configName].isArray())
	{
		bool _match = _config[configName].some([&](jsoncpp& item)
			{
				if (item["ps"].isString())
				{
					if (psName.compare_icase(item["ps"].asString()))
					{
						return true;
					}
				}
				return false;
			});

		return _match;
	}

	return false;
}

void easyWDM::WndHookProc(HWND hWnd, bool isCreate)
{
	if (!hWnd) return;

	auto hCurMonitor = helper::getCurrentMonitor();

	helper::showOwerWnd(hCurMonitor, false);

	//����
	auto txtName = helper::getWndTitle(hWnd);
	auto className = helper::getWndClass(hWnd);
	eString psName = helper::getProcessName(hWnd);
	
	if (isCreate)
	{
	}
	console.log("{}:{:08X} - {} -��{} - {}", isCreate?"����":"��ʾ",(DWORD)hWnd, txtName, className, psName);

	//������Ҫ���صĴ���
	if (is_match("hook_windows_hide", psName, txtName, className))
	{
		console.log("���ش���:{:08X} - {} -��{} - {}", (DWORD)hWnd, txtName, className, psName);
		::ShowWindow(hWnd, SW_HIDE);
		return;
	}

	//������Ҫ�ŶӵĴ���
	if (is_match("hook_windows_filter", psName, txtName, className))
	{
		return;
	}
	
	//console.log("�����:{} - {}", className, txtName);

	//ֻת�ƴ�����5������ʾ�Ĵ���
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

	//ÿ��MAX_WAIT_TIME*2 ����һ��map
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
		console.error("�õ�������ʾ�����ʧ��:{}", className);
		return;
	}

	RECT monRct = { 0 };
	if (!helper::getCurrentMonitorRecv(&monRct)) return;

	//����Ҫ����λ��

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

	if (!psName.empty())
	{

	}

	bool isMax = ::IsZoomed(hWnd);

	//auto ret = SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
	auto ret = SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	if (isMax) ::ShowWindow(hWnd, SW_SHOWMAXIMIZED);

	//SWP_DRAWFRAME
	console.log("ת����ʾ��-(PS:{}) ({}): {}*{}", psName, className, (int)(x - monRct.left), (int)(y - monRct.top));
}