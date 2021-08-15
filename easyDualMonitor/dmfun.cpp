#include "dmfun.h"
#include "tray_icon.h"
#include <thread>
#include <dwmapi.h>
#include <easy/easy.h>

using namespace easy;

#pragma comment(lib, "Dwmapi.lib")

#define EASY_ATOM "easy_monitor_atom"

//�Ƿ��������
static bool		g_enable_mouse_limit = false;

//�Ƿ���ʱ�ر�����
static bool		g_temp_close_limit = false;

//��ʶ�Ƿ���������������
static bool g_limit_is_set_rect = false;

//�Ƿ����˿��ư���
inline bool is_DownControlKey()
{
	return (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		|| (GetAsyncKeyState(VK_MENU) & 0x8000)
		|| (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
}

inline bool is_DownKey(UINT key)
{
	return (GetAsyncKeyState(key) & 0x8000);
}

bool dmfun::DispatchMouseEvent(_MOUSE_BUTTON button, POINT pt)
{
	//��ǰ��������
	static RECT g_limit_rect = { 0 };

	static bool s_lbutton_is_down = false;

	static HWND s_wnd_lock = nullptr;

	//��������м�-Alt+�м� ��ס����,Ctrl+�м� �ָ���ס����
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

			//�Ŷ��������������б��а��м�
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

	//��ס���,�����Ҽ�-��ʱ�����������
	if (g_enable_mouse_limit && button == EWM_RBUTTONDOWN && s_lbutton_is_down)
	{
		g_temp_close_limit = !g_temp_close_limit;
		console.log("��ʱ{}�������", g_temp_close_limit);

		if (g_temp_close_limit) ::ClipCursor(nullptr);
		return true;
	}

	//�Ƿ���������� ���� û�б���ʱ�ر�
	if (button == EWM_MOUSEMOVE && g_enable_mouse_limit && !g_temp_close_limit)
	{
		//�ж��Ƿ���������������
		if (!g_limit_is_set_rect)
		{
			//���������Ctrl �� Win �� Alt������.
			if (is_DownControlKey()) return false;

			if (getCurrentMonitorRecv(&g_limit_rect))
			{
				g_limit_rect.right -= 2;
				console.debug("�����˵�ǰ��ʾ��");
				g_limit_is_set_rect = true;
			}
			else {
				return false;
			}
		}

		//ȫ��ȡ����ջ
		LONG x = pt.x;
		LONG y = pt.y;
		if (x < g_limit_rect.left) x = g_limit_rect.left;
		else if (x > g_limit_rect.right) x = g_limit_rect.right;
		if (y < g_limit_rect.top) y = g_limit_rect.top;
		else if (y > g_limit_rect.bottom) y = g_limit_rect.bottom;

		//��������.
		if (x != pt.x || y != pt.y)
		{
			if (is_DownControlKey())
			{
				g_limit_is_set_rect = false;
				::ClipCursor(nullptr);
			}
			else {
				::ClipCursor(&g_limit_rect);
			}
		}
	}

	return false;
}

static bool IsInvisibleWin10BackgroundAppWindow(HWND hWnd)
{
	int CloakedVal;
	HRESULT hRes = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &CloakedVal, sizeof(CloakedVal));
	if (hRes != S_OK) CloakedVal = 0;
	return CloakedVal ? true : false;
}

void dmfun::ShowRunDlg()
{
	//�������߳���ʾ�Ի���
	if (::GetCurrentThreadId() == ::GetWindowThreadProcessId(_tray.GetWnd(), nullptr))
	{
		std::thread thread_(&dmfun::ShowRunDlg, this);
		thread_.detach();
		return;
	}

	RECT rct = { 0 };
	getCurrentMonitorRecv(&rct);

	//�����ھ��,ͼ��,һ��δ֪·��,���ڱ���,˵������,δ֪(������ʾΪ0x14��0x4)
	typedef DWORD(WINAPI* LPRUNDLG)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, DWORD);
	LPRUNDLG RunDlg = nullptr;
	HMODULE hMod = ::LoadLibrary("shell32.dll");
	if (hMod)RunDlg = (LPRUNDLG)::GetProcAddress(hMod, MAKEINTRESOURCE(61));

	if (!RunDlg)
	{
		show_error("��ȡshell32.#61������ַʧ��!");
		return;
	}

	HWND hSBWnd = nullptr;

	hSBWnd = CreateWindowA("Static", nullptr, 0, rct.left, rct.bottom - 300, 300, 300, nullptr, nullptr, (HINSTANCE)::GetModuleHandleA(NULL), NULL);

	DWORD dwResult = RunDlg(hSBWnd, nullptr, nullptr, nullptr, nullptr, 4);
	::FreeLibrary(hMod);

	::DestroyWindow(hSBWnd);
	return;
}
void dmfun::ShowDisktop()
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

			//����ʱ������
		#ifdef _DEBUG
			//if (className == "HwndWrapper[DefaultDomain;;4da6d4be-d0ca-41a1-b9a9-bf651e51960c]") continue;
		#endif // _DEBUG

			//����ж���������Ƿ��������ʾ����
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

	//����д�������ʾ
	if (!vecCur.empty())
	{
		vecOld.clear();
		for (auto& wnd : vecCur)
		{
			//::ShowWindow(wnd, SW_MINIMIZE);		//��С������
			vecOld.push_back({ wnd,(bool)::IsZoomed(wnd) });

			if (!IsWindowVisible(wnd)) continue;

			ShowOwnedPopups(wnd, false);
			::ShowWindow(wnd, SW_MINIMIZE);		//��С������
		}

		//��������ǰ̨ȡ�ý���
		activeWnd(hDesktop);
	}
	else {
		HWND hLastWnd = nullptr;

		//��ԭ����
		for (auto& info : vecOld)
		{
			if (!::IsWindow(info.wnd)) continue;

			//if (!IsIconic(info.wnd) && IsWindowVisible(info.wnd)) continue;

			::ShowWindow(info.wnd, SW_SHOWNOACTIVATE);
			ShowOwnedPopups(info.wnd, true);

			hLastWnd = info.wnd;
		}
		vecOld.clear();

		//�����ԭ�Ĵ�����ǰ̨����
		activeWnd(hLastWnd);
	}
}

void dmfun::activeWnd(HWND hWnd)
{
	if (hWnd == nullptr) return;

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

bool dmfun::hook_wnd(bool is_enable)
{
	if (is_enable)
	{
		static UINT reg_shell = 0;
		if (reg_shell == 0)
		{
			reg_shell = RegisterWindowMessageA("SHELLHOOK");

			if (reg_shell == 0)
			{
				auto errstr = fmt::format("RegisterWindowMessageA Failure,{}", ::GetLastError());
				show_error(errstr.c_str());
				return false;
			}
		}

		if (!RegisterShellHookWindow(_tray.GetWnd()))
		{
			auto errstr = fmt::format("RegisterShellHookWindow Failure,{}", ::GetLastError());
			show_error(errstr.c_str());
			return false;
		}

		_tray.set_msg_handler(reg_shell, [&](WPARAM wParam, LPARAM lParam)
			{
				if (wParam == HSHELL_WINDOWCREATED)
				{
					WndHookProc((HWND)lParam, true);
				}
				else if (wParam == HSHELL_WINDOWACTIVATED
					|| wParam == HSHELL_RUDEAPPACTIVATED)
				{
					WndHookProc((HWND)lParam, false);
				}
				else if (wParam == HSHELL_MONITORCHANGED) {
					//console.log("class:{} HSHELL_MONITORCHANGED",  getWndClass((HWND)lParam));
				}
				else {
					//console.log("param:{},class:{}", wParam,getWndClass((HWND)lParam));
				}
			});
	}
	else {
		DeregisterShellHookWindow(_tray.GetWnd());
	}

	return true;
}
bool dmfun::set_limit_mouse(bool is_enable)
{
	if (is_enable)
	{
		if (!_installMouseHook()) return false;
	}
	else {
		if (!_uninstallMouseHook()) return false;
	}

	g_enable_mouse_limit = is_enable;
	g_limit_is_set_rect = false;
	g_temp_close_limit = false;

	if (!g_enable_mouse_limit) ::ClipCursor(nullptr);
	return true;
}

bool dmfun::show_StartMenu()
{
	HWND hStartWnd = GetCurrentMonitorStartMenuWnd();
	if (!hStartWnd) return false;

	HWND hWnd = GetParent(hStartWnd);
	HWND hForeWnd;
	DWORD dwForeID;
	DWORD dwCurID;
	hForeWnd = GetForegroundWindow();
	dwCurID = GetCurrentThreadId();
	dwForeID = GetWindowThreadProcessId(hForeWnd, nullptr);
	AttachThreadInput(dwCurID, dwForeID, TRUE);
	ShowWindow(hWnd, SW_SHOWNORMAL);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	SetForegroundWindow(hWnd);
	AttachThreadInput(dwCurID, dwForeID, FALSE);

	//::PostMessage(GetParent(hWnd), WM_COMMAND, MAKELONG(401, 0), NULL);

	PostMessageA(hStartWnd, WM_MOUSEMOVE, 0, MAKELONG(30, 30));
	PostMessageA(hStartWnd, WM_LBUTTONDOWN, 0, MAKELONG(30, 30));
	PostMessageA(hStartWnd, WM_LBUTTONUP, 0, MAKELONG(30, 30));

	return true;
}

bool dmfun::_installMouseHook()
{
	if (_hookMouse==nullptr)
	{
		_hookMouse = SetWindowsHookExA(WH_MOUSE_LL, MouseHookProc, ::GetModuleHandleA(nullptr), NULL);
		if (!_hookMouse)
		{
			auto errstr = fmt::format("SetWindowsHookEx Failure,err code:{}", GetLastError());
			show_error(errstr.c_str());
		}
		return _hookMouse != nullptr;
	}
	else {
		return true;
	}
}

bool dmfun::_uninstallMouseHook()
{
	if (_hookMouse)
	{
		if (!UnhookWindowsHookEx(_hookMouse))
		{
			show_error("UnhookWindowsHookEx Failure");
		}
		_hookMouse = nullptr;
		return true;
	}
	else {
		return true;
	}
}

LRESULT CALLBACK dmfun::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0 || !wParam || !lParam) return CallNextHookEx(_hookMouse, nCode, wParam, lParam);

	if (DispatchMouseEvent((_MOUSE_BUTTON)wParam, ((MOUSEHOOKSTRUCT*)lParam)->pt))
	{
		return true;
	}
	else {
		return CallNextHookEx(_hookMouse, nCode, wParam, lParam);
	}
}

bool dmfun::DispatchKeyEvent(UINT key, KBDLLHOOKSTRUCT* pHook)
{
	//��ʼ�˵�
	if (key== WM_KEYUP)
	{
		if (pHook->vkCode == VK_LWIN)
		{
			show_StartMenu();
			return true;
		}
		//console.log("{}", key);
	}
	return false;
}


LRESULT CALLBACK dmfun::KeyHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0 || !wParam || !lParam) return CallNextHookEx(_hookMouse, nCode, wParam, lParam);

	if (DispatchKeyEvent(wParam, (KBDLLHOOKSTRUCT*)lParam))
	{
		return true;
	}
	else {
		return CallNextHookEx(_hookKey, nCode, wParam, lParam);
	}
}


bool dmfun::_installKeyHook()
{
	if (_hookKey == nullptr)
	{
		_hookKey = SetWindowsHookExA(WH_KEYBOARD_LL, KeyHookProc, ::GetModuleHandleA(nullptr), NULL);
		if (!_hookKey)
		{
			auto errstr = fmt::format("SetWindowsHookEx Failure,err code:{}", GetLastError());
			show_error(errstr.c_str());
		}
		return _hookKey != nullptr;
	}
	else {
		return true;
	}
}

void dmfun::WndHookProc(HWND hWnd, bool isCreate)
{
	//����ж���������Ƿ��������ʾ����

	//ֻת�ƴ�����3������ʾ�Ĵ���
#define MAX_WAIT_TIME 5
	static DWORD s_last_clear_tick = (DWORD)(::GetTickCount64() / 1000);

	DWORD dwCurTick = (DWORD)(::GetTickCount64() / 1000);

	if (isCreate)
	{
		_mapWndTick[hWnd] = dwCurTick;
	}
	else {
		//console.log("��ʾ:{}", getWndClass(hWnd));

		auto it = _mapWndTick.find(hWnd);
		if (it == _mapWndTick.end()) return;

		DWORD dwCreateTick = it->second;
		_mapWndTick.erase(hWnd);

		DWORD dwTick = (DWORD)(::GetTickCount64() / 1000);
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

	auto hMonitor = getCurrentMonitor();

	RECT rct = { 0 };
	if (!::GetWindowRect(hWnd, &rct)) return;

	if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) == hMonitor) return;

	RECT monRct = { 0 };
	if (!this->getCurrentMonitorRecv(&monRct)) return;

	//����Ҫ����λ��

	int wndWidth = rct.right - rct.left;
	int wndHeight = rct.bottom - rct.top;

	int monWidth = monRct.right - monRct.left;
	int monHeight = monRct.bottom - monRct.top;

	bool is_handler = false;

	int x = 0, y = 0;

	//�������д���
	if (wndWidth < 500 && wndWidth >= 400 && wndHeight <= 300 && wndHeight >= 200
		&& getWndTitle(hWnd) == "����" && getWndClass(hWnd) == "#32770")
	{
		x = monRct.left;
		y = monRct.bottom - wndHeight;

		HWND hTaskBar = GetCurrentMonitorStartMenuWnd();
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
					//������
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
	}

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

	console.log("�޸�λ��-�������:{}*{}", (int)(x - monRct.left), (int)(y - monRct.top));
}

void dmfun::show_error(const char* str)
{
	console.warn("{}", str);
	_tray.show_err(str);
}
HWND dmfun::GetDesktopWnd()
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
	show_error("����������ʧ��!");
	return nullptr;
}
HWND dmfun::GetCurrentMonitorStartMenuWnd()
{
	HWND hWnd = nullptr;

	//�������п�ʼ�˵���ť
	auto _findStart = [](std::vector<HWND>& vec, const char* clsName)
	{
		HWND hLastWnd = nullptr;
		do
		{
			hLastWnd = FindWindowExA(nullptr, hLastWnd, clsName, nullptr);
			if (hLastWnd)
			{
				HWND hWnd = ::FindWindowExA(hLastWnd, nullptr, "Start", "��ʼ");
				if (hWnd) vec.push_back(hWnd);
			}
		} while (hLastWnd);
	};

	//Shell_TrayWnd,Shell_SecondaryTrayWnd;
	//ö�����п�ʼ��ť
	std::vector<HWND> vec;
	_findStart(vec, "Shell_TrayWnd");
	_findStart(vec, "Shell_SecondaryTrayWnd");

	//�õ���ǰ���������
	auto hMon = getCurrentMonitor();

	for (auto wnd : vec)
	{
		if (MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST) == hMon)
		{
			hWnd = wnd;
			break;
		}
	}

	return hWnd;
}
HMONITOR dmfun::getCurrentMonitor()
{
	POINT pos = { 0 };
	if (!GetCursorPos(&pos))
	{
		return nullptr;
	}
	return MonitorFromPoint(pos, MONITOR_DEFAULTTONEAREST);
}

bool dmfun::getCurrentMonitorRecv(RECT* lpRect)
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

dmfun::dmfun(tray_icon& tray)
	:_tray(tray)
{
	//_installMouseHook();
	
	_tray.set_msg_handler(WM_DISPLAYCHANGE, [&]()
		{
			//����ϢΪ���ӷֱ��ʱ��,������ʾ��ʱҲ����������Ϣ
			g_limit_is_set_rect = false;

			//�л���ʾ��ʱ��Ҫ��մ����б�
			_mapWndTick.clear();
		});

	_installKeyHook();

}

dmfun::~dmfun()
{
	_uninstallMouseHook();
}

bool dmfun::modify_explorer_hotkey(const char* key, bool enable)
{
	auto reg_path = R"(�����\HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced)";
	etd::string value, oldvalue;
	if (!util::read_reg_string(reg_path, "DisabledHotkeys", value))
	{
		show_error("��ѯע�����Ϣʧ��");
		return false;
	}
	oldvalue = value;
	if (enable)
	{
		if (value.find(key) != std::string::npos) return true;
		value += key;
	}
	else {
		if (value.find(key) == std::string::npos) return true;
		value.replaceAll(key, "");
	}

	if (::MessageBoxA(::GetDesktopWindow(), "��Ҫ������Դ������(explorer.exe����),�Ƿ����", "ȷ��?", MB_YESNO) != IDYES)
	{
		return false;
	}

	if (!util::write_reg_string(reg_path, "DisabledHotkeys", value))
	{
		show_error("д��ע�����Ϣʧ��");
		return false;
	}

	if (!util::restart_process("explorer.exe"))
	{
		util::write_reg_string(reg_path, "DisabledHotkeys", oldvalue);	//��ԭע���ֵ
		show_error("������Դ������ʧ��");
		return false;
	}

	return true;
}

HWND dmfun::_GetCursorWnd()
{
	POINT pt;
	if (GetPhysicalCursorPos(&pt))
	{
		return WindowFromPhysicalPoint(pt);
	}
	return nullptr;
}

HWND dmfun::_GetCursorTopWnd()
{
	HWND hWnd = _GetCursorWnd();
	HWND hParent = ::GetParent(hWnd);
	while (hParent != nullptr)
	{
		hWnd = hParent;
		hParent = ::GetParent(hWnd);
	}
	return hWnd;
}
