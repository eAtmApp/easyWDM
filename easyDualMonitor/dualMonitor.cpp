#include "dualMonitor.h"
#include "tray_icon.h"
#include <thread>
#include <dwmapi.h>

#pragma comment(lib, "Dwmapi.lib")

#define EASY_ATOM "easy_monitor_atom"

static bool IsInvisibleWin10BackgroundAppWindow(HWND hWnd)
{
	int CloakedVal;
	HRESULT hRes = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &CloakedVal, sizeof(CloakedVal));
	if (hRes != S_OK) CloakedVal = 0;
	return CloakedVal ? true : false;
}

void dualMonitor::ShowRunDlg()
{
	//不在主线程显示对话框
	if (::GetCurrentThreadId() == ::GetWindowThreadProcessId(_tray.get_hwnd(), nullptr))
	{
		std::thread thread_(&dualMonitor::ShowRunDlg, this);
		thread_.detach();
		return;
	}

	RECT rct = { 0 };
	getCurrentMonitorRecv(&rct);

	//父窗口句柄,图标,一个未知路径,窗口标题,说明文字,未知(跟踪显示为0x14或0x4)
	typedef DWORD(WINAPI* LPRUNDLG)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, DWORD);
	LPRUNDLG RunDlg = nullptr;
	HMODULE hMod = ::LoadLibrary("shell32.dll");
	if (hMod)RunDlg = (LPRUNDLG)::GetProcAddress(hMod, MAKEINTRESOURCE(61));

	if (!RunDlg)
	{
		show_error("获取shell32.#61函数地址失败!");
		return;
	}

	HWND hSBWnd = nullptr;

	hSBWnd = CreateWindowA("Static", nullptr, 0, rct.left, rct.bottom - 300, 300, 300, nullptr, nullptr, (HINSTANCE)::GetModuleHandleA(NULL), NULL);

	DWORD dwResult = RunDlg(hSBWnd, nullptr, nullptr, nullptr, nullptr, 4);
	::FreeLibrary(hMod);

	::DestroyWindow(hSBWnd);
	return;
}

void dualMonitor::ShowDisktop()
{
	struct _WND_RC
	{
		HWND wnd;
		bool isMax;
	};
	static std::map<HMONITOR, std::vector<_WND_RC>> s_map_rc;

	HWND hDesktop = GetDesktopWnd();

	HMONITOR hMonitor = getCurrentMonitor();

	auto EnumMonitorWnd = [this](HWND hDesktop, HMONITOR hMonitor)
	{
		std::vector<HWND> vec;

		HWND hWndCCC = hDesktop;

		while (hWndCCC)
		{
			hWndCCC = GetNextWindow(hWndCCC, GW_HWNDPREV);
			if (!::IsWindowVisible(hWndCCC) || IsIconic(hWndCCC)
				|| IsInvisibleWin10BackgroundAppWindow(hWndCCC))
			{
				continue;
			}

			auto className = getWndClass(hWndCCC);
			auto titleName = getWndTitle(hWndCCC);

			if (className == "Shell_SecondaryTrayWnd"
				|| className == "Shell_TrayWnd"
				|| className == "WorkerW")
			{
				continue;
			}

			//最后判断这个窗口是否在这个显示器中
			RECT rct = { 0 };
			if (::GetWindowRect(hWndCCC, &rct)
				&& MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) != hMonitor)
			{
				continue;
			}

			vec.push_back(hWndCCC);
		}
		return vec;
	};

	auto vecCur = EnumMonitorWnd(hDesktop, hMonitor);

	std::vector<_WND_RC>* pVec = nullptr;
	auto itor = s_map_rc.find(hMonitor);
	if (itor == s_map_rc.end())
	{
		std::vector<_WND_RC> rc;
		s_map_rc[hMonitor] = rc;

		itor = s_map_rc.find(hMonitor);
	}
	pVec = &itor->second;
	auto& vecOld = *pVec;

	//如果有窗口在显示
	if (!vecCur.empty())
	{
		vecOld.clear();
		for (auto& wnd : vecCur)
		{
			::ShowWindow(wnd, SW_MINIMIZE);		//最小化窗口
			vecOld.push_back({ wnd,(bool)::IsZoomed(wnd) });
		}

		//将桌面置前台取得焦点
		{
			HWND hWnd = hDesktop;
			HWND hForeWnd = NULL;
			DWORD dwForeID;
			DWORD dwCurID;
			hForeWnd = GetForegroundWindow();
			dwCurID = GetCurrentThreadId();
			dwForeID = GetWindowThreadProcessId(hForeWnd, NULL);
			AttachThreadInput(dwCurID, dwForeID, TRUE);
			ShowWindow(hWnd, SW_SHOWNORMAL);
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			SetForegroundWindow(hWnd);
			AttachThreadInput(dwCurID, dwForeID, FALSE);
		}
	}
	else {
		HWND hLastWnd = nullptr;

		//还原窗口
		for (auto& info : vecOld)
		{
			::ShowWindow(info.wnd, info.isMax ? SW_SHOWMAXIMIZED : SW_RESTORE);
			hLastWnd = info.wnd;
		}
		vecOld.clear();

		//将最后还原的窗口置前台激活
		if (hLastWnd != nullptr)
		{
			HWND hWnd = hLastWnd;
			HWND hForeWnd = NULL;
			DWORD dwForeID;
			DWORD dwCurID;
			hForeWnd = GetForegroundWindow();
			dwCurID = GetCurrentThreadId();
			dwForeID = GetWindowThreadProcessId(hForeWnd, NULL);
			AttachThreadInput(dwCurID, dwForeID, TRUE);
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			SetForegroundWindow(hWnd);
			AttachThreadInput(dwCurID, dwForeID, FALSE);
		}
	}
}

bool dualMonitor::hookWnd(bool is_enable)
{
	if (is_enable)
	{
		static UINT reg_shell = 0;
		if (reg_shell == 0) reg_shell = RegisterWindowMessageA("SHELLHOOK");

		if (reg_shell == 0)
		{
			show_error("RegisterWindowMessage Failure");
			return false;
		}

		if (!RegisterShellHookWindow(_tray.get_hwnd()))
		{
			show_error("RegisterShellHookWindow Failure");
			return false;
		}

		_tray.set_msg_handler(reg_shell, [&](WPARAM wParam, LPARAM lParam)
			{
				if (wParam == HSHELL_WINDOWCREATED)
				{
					hook_wnd_handler((HWND)lParam, true);
				}
				else if (wParam == HSHELL_WINDOWACTIVATED
					|| wParam == HSHELL_RUDEAPPACTIVATED)
				{
					hook_wnd_handler((HWND)lParam, false);
				}
			});
	}
	else {
		DeregisterShellHookWindow(_tray.get_hwnd());
	}

	return true;
}

RECT _limit_rect = { 0 };
bool _is_limit_rect = false;

HWND _last_wnd = nullptr;

//是否按下了控制按键
inline bool is_DownControlKey()
{
	return (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		|| (GetAsyncKeyState(VK_MENU) & 0x8000)
		|| (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
}

LRESULT CALLBACK dualMonitor::mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0 || wParam != WM_MOUSEMOVE || !wParam || !lParam) return CallNextHookEx(_hookMouse, nCode, wParam, lParam);

	if (!_is_limit_rect)
	{
		//如果按下了Ctrl则不限制.
		if (is_DownControlKey())
		{
			return CallNextHookEx(_hookMouse, nCode, wParam, lParam);
		}
		
		if (getCurrentMonitorRecv(&_limit_rect))
		{
			_is_limit_rect = true;
		}
		else {
			return CallNextHookEx(_hookMouse, nCode, wParam, lParam);
		}
	}

	auto pMouseHook = (MOUSEHOOKSTRUCT*)lParam;

	//全部取出到栈
	LONG x = pMouseHook->pt.x;
	LONG y = pMouseHook->pt.y;

	if (x < _limit_rect.left) x = _limit_rect.left;
	if (x > _limit_rect.right) x = _limit_rect.right;

	if (y < _limit_rect.top) y = _limit_rect.top;
	if (y > _limit_rect.bottom) y = _limit_rect.bottom;

	//被限制了.
	if (x != ((MOUSEHOOKSTRUCT*)lParam)->pt.x || y != ((MOUSEHOOKSTRUCT*)lParam)->pt.y)
	{
		if (is_DownControlKey())
		{
			_is_limit_rect = false;
			::ClipCursor(nullptr);
		}
		else {
			::ClipCursor(&_limit_rect);
		}
	}
	return CallNextHookEx(_hookMouse, nCode, wParam, lParam);
}

bool dualMonitor::limitMouse(bool is_enable)
{
	if (is_enable)
	{
		_hookMouse = SetWindowsHookExA(WH_MOUSE_LL, mouseProc, ::GetModuleHandleA(nullptr), NULL);
		if (_hookMouse == nullptr)
		{
			show_error("SetWindowsHookEx Failure");
		}
		return _hookMouse != nullptr;
	}
	else {
		if (!UnhookWindowsHookEx(_hookMouse))
		{
			show_error("UnhookWindowsHookEx Failure");
			return false;
		}
		_hookMouse = nullptr;
		return true;
	}
}

void dualMonitor::hook_wnd_handler(HWND hWnd, bool isCreate)
{
	//最后判断这个窗口是否在这个显示器中

	//只转移创建后3秒内显示的窗口
#define MAX_WAIT_TIME 3
	static DWORD s_last_clear_tick = (DWORD)(::GetTickCount64() / 1000);

	DWORD dwCurTick = (DWORD)(::GetTickCount64() / 1000);

	static std::map<HWND, DWORD> _mapTick;

	if (isCreate)
	{
		_mapTick[hWnd] = dwCurTick;
		return;
	}
	else {
		auto it = _mapTick.find(hWnd);
		if (it == _mapTick.end()) return;

		DWORD dwCreateTick = it->second;
		_mapTick.erase(hWnd);

		DWORD dwTick = (DWORD)(::GetTickCount64() / 1000);
		if (dwCurTick - dwCreateTick > MAX_WAIT_TIME) return;
	}

	//每隔MAX_WAIT_TIME*2 清理一次map
	if (dwCurTick - s_last_clear_tick > MAX_WAIT_TIME * 2)
	{
		for (auto it = _mapTick.begin(); it != _mapTick.end();)
		{
			auto itbak = it;
			auto key = it->first;
			auto val = it->second;
			++it;
			if (dwCurTick - val > MAX_WAIT_TIME)
			{
				_mapTick.erase(itbak);
			}
		}
	}

	auto hMonitor = getCurrentMonitor();

	RECT rct = { 0 };
	if (!::GetWindowRect(hWnd, &rct)) return;

	if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) == hMonitor) return;

	RECT monRct = { 0 };
	if (!this->getCurrentMonitorRecv(&monRct)) return;

	//这里要计算位置

	int wndWidth = rct.right - rct.left;
	int wndHeight = rct.bottom - rct.top;

	int monWidth = monRct.right - monRct.left;
	int monHeight = monRct.bottom - monRct.top;

	POINT pos;
	if (!GetCursorPos(&pos)) return;

	int x = 0, y = 0;
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

	auto ret = SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);

	//console.log("修改位置-相对坐标:{}*{}", (int)(x - monRct.left), (int)(y - monRct.top));
}

void dualMonitor::show_error(const char* str)
{
	_tray.show_err(str);
}
HWND dualMonitor::GetDesktopWnd()
{
	HWND hWnd = ::FindWindowA("Progman", "Program Manager");
	if (::FindWindowExA(hWnd, nullptr, "SHELLDLL_DefView", nullptr) != nullptr) return hWnd;

	hWnd = nullptr;
	do
	{
		hWnd = ::FindWindowEx(nullptr, hWnd, "WorkerW", nullptr);
		if (hWnd == nullptr) break;

		if (::FindWindowExA(hWnd, nullptr, "SHELLDLL_DefView", nullptr) != nullptr) return hWnd;
	} while (true);
	show_error("查找桌面句柄失败!");
	return nullptr;
}
HWND dualMonitor::GetCurrentMonitorStartMenuWnd()
{
	HWND hWnd = nullptr;

	//查找所有开始菜单按钮
	auto _findStart = [](std::vector<HWND>& vec, const char* clsName)
	{
		HWND hLastWnd = nullptr;
		do
		{
			hLastWnd = FindWindowExA(nullptr, hLastWnd, clsName, nullptr);
			if (hLastWnd)
			{
				HWND hWnd = ::FindWindowExA(hLastWnd, nullptr, "Start", "开始");
				if (hWnd) vec.push_back(hWnd);
			}
		} while (hLastWnd);
	};

	//Shell_TrayWnd,Shell_SecondaryTrayWnd;
	//枚举所有开始按钮
	std::vector<HWND> vec;
	_findStart(vec, "Shell_TrayWnd");
	_findStart(vec, "Shell_SecondaryTrayWnd");

	//得到当前监视器句柄
	auto hMon = getCurrentMonitor();

	for (auto wnd : vec)
	{
		if (MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST) == hMon)
		{
			hWnd = wnd;
			break;
		}
	}

	if (!hWnd)
	{
		show_error("查找开始按钮句柄失败!");
	}

	return hWnd;
}
HMONITOR dualMonitor::getCurrentMonitor()
{
	POINT pos = { 0 };
	if (!GetCursorPos(&pos))
	{
		return nullptr;
	}
	return MonitorFromPoint(pos, MONITOR_DEFAULTTONEAREST);
}

bool dualMonitor::getCurrentMonitorRecv(RECT* lpRect)
{
	auto hMon = getCurrentMonitor();

	MONITORINFOEX moninfo = { 0 };
	moninfo.cbSize = sizeof(moninfo);

	if (!GetMonitorInfoA(hMon, &moninfo))
	{
		return false;
	}
	memcpy(lpRect, &moninfo.rcMonitor, sizeof(RECT));
	return true;
}

dualMonitor::dualMonitor(tray_icon& tray)
	:_tray(tray)
{
}

dualMonitor::~dualMonitor()
{
}

