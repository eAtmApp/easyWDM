#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>

class tray_icon;
class dualMonitor
{
public:
	dualMonitor(tray_icon& tray);
	~dualMonitor();

	bool	replace_ShowDesktop(bool enable);

	bool	replace_ShowRun(bool enable);

	void	ShowRunDlg();
	
	void	ShowDisktop();
	
	bool	hook_wnd(bool is_enable);
	
	bool	set_limit_mouse(bool is_enable);
	
	static void	show_StartMenu();

	bool	installMouseHook();
private:
	
	//�õ�����µĴ��ھ��
	static HWND	_GetCursorWnd();

	static HWND _GetCursorTopWnd();

	//�����
	static void	activeWnd(HWND hWnd);
	
	bool	modify_explorer_hotkey(const char* key, bool enable);

	void	WndHookProc(HWND hWnd, bool isCreate);

	void	show_error(const char* str);
	
	//ȡ��������
	HWND GetDesktopWnd();

	//��ǰ��������ʼ�˵����
	static HWND GetCurrentMonitorStartMenuWnd();

	//ȡ�õ�ǰ������
	static HMONITOR getCurrentMonitor();

	//ȡ�õ�ǰ����������
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
	static bool	DispatchMouseEvent(_MOUSE_BUTTON button,POINT pt);

	static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

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
	
	std::map<HWND, DWORD> _mapWndTick;

	tray_icon& _tray;
};

