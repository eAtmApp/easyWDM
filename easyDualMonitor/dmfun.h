#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>

class tray_icon;
class dmfun
{
public:
	dmfun(tray_icon& tray);
	~dmfun();

	bool	replace_ShowDesktop(bool enable);

	bool	replace_ShowRun(bool enable);

	void	ShowRunDlg();

	void	ShowDisktop();

	bool	hook_wnd(bool is_enable);

	bool	set_limit_mouse(bool is_enable);

	static bool	show_StartMenu();

	bool	_installMouseHook();
	bool	_uninstallMouseHook();

	bool	_installKeyHook();
public:

	//得到鼠标下的窗口句柄
	static HWND	_GetCursorWnd();

	static HWND _GetCursorTopWnd();

	//激活窗口
	static void	activeWnd(HWND hWnd);

	bool	modify_explorer_hotkey(const char* key, bool enable);

	void	WndHookProc(HWND hWnd, bool isCreate);

	void	show_error(const char* str);

	//取得桌面句柄
	HWND GetDesktopWnd();

	//当前监视器开始菜单句柄
	static HWND GetCurrentMonitorStartMenuWnd();

	//取得当前监视器
	static HMONITOR getCurrentMonitor();

	//取得当前监视器矩阵
	static bool getCurrentMonitorRecv(RECT* lpRect);

	enum _MOUSE_BUTTON
	{
		EWM_MOUSEFIRST = 0x0200,
		EWM_MOUSEMOVE = 0x0200,
		EWM_LBUTTONDOWN = 0x0201,
		EWM_LBUTTONUP = 0x0202,
		EWM_LBUTTONDBLCLK = 0x0203,
		EWM_RBUTTONDOWN = 0x0204,
		EWM_RBUTTONUP = 0x0205,
		EWM_RBUTTONDBLCLK = 0x0206,
		EWM_MBUTTONDOWN = 0x0207,
		EWM_MBUTTONUP = 0x0208,
		EWM_MBUTTONDBLCLK = 0x0209
	};

	static bool	DispatchMouseEvent(_MOUSE_BUTTON button, POINT pt);

	static bool	DispatchKeyEvent(UINT key, KBDLLHOOKSTRUCT* pHook);

	static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK KeyHookProc(int nCode, WPARAM wParam, LPARAM lParam);

	static std::string getWndClass(HWND hWnd)
	{
		std::string result;
		result.resize(256);
		auto size = RealGetWindowClassA(hWnd, result.data(), 256);
		result.resize(size);
		return result;
	}
	static std::string getWndTitle(HWND hWnd)
	{
		std::string result;
		result.resize(256);
		auto size = GetWindowTextA(hWnd, result.data(), 256);
		result.resize(size);
		return result;
	}

private:
	inline static HHOOK	_hookMouse = nullptr;
	inline static HHOOK	_hookKey = nullptr;

	std::map<HWND, DWORD> _mapWndTick;

	tray_icon& _tray;
};

