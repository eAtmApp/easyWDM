#pragma once
#include <windows.h>
#include <string>
#include <vector>

class tray_icon;
class dualMonitor
{
public:
	dualMonitor(tray_icon& tray);
	~dualMonitor();

	void ShowRunDlg();
	
	void ShowDisktop();

	bool	hookWnd(bool is_enable);
	
	bool	limitMouse(bool is_enable);
private:
	
	void hook_wnd_handler(HWND hWnd, bool isCreate);

	void	show_error(const char* str);
	
	//取得桌面句柄
	HWND GetDesktopWnd();

	//当前监视器开始菜单句柄
	HWND GetCurrentMonitorStartMenuWnd();

	//取得当前监视器
	static HMONITOR getCurrentMonitor();

	//取得当前监视器矩阵
	static bool getCurrentMonitorRecv(RECT* lpRect);

	static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);

	std::string getWndClass(HWND hWnd)
	{
		std::string result;
		result.resize(256);
		auto size = RealGetWindowClassA(hWnd, result.data(), 256);
		result.resize(size);
		return result;
	}
	std::string getWndTitle(HWND hWnd)
	{
		std::string result;
		result.resize(256);
		auto size = GetWindowTextA(hWnd, result.data(), 256);
		result.resize(size);
		return result;
	}

private:
	inline static HHOOK	_hookMouse = nullptr;

	tray_icon& _tray;
};

