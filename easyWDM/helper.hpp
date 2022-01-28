
#include <easy/easy.h>
#include <windows.h>
#include <dwmapi.h>
#include <combaseapi.h>
#include <shldisp.h>

using namespace easy;

class helper
{
public:

	inline static	HICON	m_runDlgIcon = nullptr;

	static bool ShowRunDlg(bool is_thread = false)
	{
		/*
				//不在主线程显示对话框
				if (!is_thread)
				{
					std::thread thread_(helper::ShowRunDlg, true);
					thread_.detach();
					return true;
				}
				HRESULT hr;
				hr=::CoInitialize(nullptr);
				{
					IShellDispatch* pShellDisp = NULL;

					hr = ::CoCreateInstance(CLSID_Shell, NULL, CLSCTX_SERVER,
						IID_IShellDispatch, (LPVOID*)&pShellDisp);

					if (hr == S_OK)
					{
						pShellDisp->FileRun();
						pShellDisp->Release();
						pShellDisp = NULL;
					}
				}
				return (hr == S_OK);*/

				//父窗口句柄,图标,工作路径,窗口标题,说明文字,未知(跟踪显示为0x14或0x4)
		typedef DWORD(WINAPI* LPRUNDLG)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, DWORD);
		static LPRUNDLG s_RunDlg = nullptr;
		if (s_RunDlg == nullptr)
		{
			HMODULE hMod = ::LoadLibrary("shell32.dll");
			if (hMod)s_RunDlg = (LPRUNDLG)::GetProcAddress(hMod, MAKEINTRESOURCE(61));
			if (!s_RunDlg) return false;
		}

		//不在主线程显示对话框
		if (!is_thread)
		{
			std::thread thread_(helper::ShowRunDlg, true);
			thread_.detach();
			return true;
		}

		RECT rct = { 0 };
		getCurrentMonitorRecv(&rct);

		HWND hSBWnd = nullptr;
		hSBWnd = CreateWindowA("Static", nullptr, 0, rct.left, rct.bottom - 300, 300, 300, nullptr, nullptr, (HINSTANCE)::GetModuleHandleA(NULL), NULL);
		if (hSBWnd)
		{
			wchar_t* homeProfile = L"USERPROFILE";
			wchar_t homePath[1024] = { 0 };
			GetEnvironmentVariableW(homeProfile, homePath, sizeof(homePath));

			activeWnd(hSBWnd);
			s_RunDlg(hSBWnd, nullptr, homePath, nullptr, nullptr, 0x4 | 0x1);
			::DestroyWindow(hSBWnd);
		}
		return hSBWnd != nullptr;
	}

	static void activeWnd(HWND hWnd)
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

	static HWND GetDesktopWnd()
	{
		HWND hWnd = ::FindWindowA("Progman", "Program Manager");
		if (::FindWindowExA(hWnd, nullptr, "SHELLDLL_DefView", nullptr) != nullptr) return hWnd;

		hWnd = nullptr;
		do
		{
			hWnd = ::FindWindowExA(nullptr, hWnd, "WorkerW", nullptr);
			if (hWnd == nullptr) break;

			if (::FindWindowExA(hWnd, nullptr, "SHELLDLL_DefView", nullptr) != nullptr) return hWnd;
		} while (true);
		return nullptr;
	}

	static bool IsInvisibleWin10BackgroundAppWindow(HWND hWnd)
	{
		int CloakedVal;
		HRESULT hRes = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &CloakedVal, sizeof(CloakedVal));
		if (hRes != S_OK) CloakedVal = 0;
		return CloakedVal ? true : false;
	}

	struct _WND_RC
	{
		HWND wnd = nullptr;
		std::string title;
		std::string cls;
		HWND hOwerWnd = nullptr;
	};

	inline static std::map<HMONITOR, std::list<_WND_RC>> _map_Wnd;

	//显示当前窗口或当前显示器的弹出窗口
	static bool showOwerWnd(HMONITOR hMonitor, bool is_active)
	{
		auto& listWnd = _map_Wnd[hMonitor];
		HWND hLastWnd = nullptr;
		for (auto it = listWnd.begin(); it != listWnd.end();)
		{
			auto cur_it = it++;
			if (cur_it->hOwerWnd)
			{
				ShowOwnedPopups(cur_it->hOwerWnd, true);
				hLastWnd = cur_it->wnd;
				listWnd.erase(cur_it);
			}
		}
		if (hLastWnd && is_active) activeWnd(hLastWnd);
		return hLastWnd != nullptr;
	}

	static bool ShowDisktop()
	{
		HWND hDesktop = GetDesktopWnd();

		HMONITOR hMonitor = getCurrentMonitor();

		if (!hDesktop || !hMonitor) return false;

		auto EnumMonitorWnd = [&](HWND hDesktop, HMONITOR hMonitor)
		{
			std::vector<_WND_RC> vec;

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
					|| className == "WorkerW"
					|| className=="SysShadow"
					|| className=="TaskListThumbnailWnd")
				{
					continue;
				}

				//调试时不隐藏
			#ifdef _DEBUG
				if (titleName.find("Microsoft Visual Studio(管理员)") != std::string::npos
					&& titleName.find("(正在") != std::string::npos)
				{
					//continue;
				}
				//if (className == "HwndWrapper[DefaultDomain;;4da6d4be-d0ca-41a1-b9a9-bf651e51960c]") continue;
			#endif // _DEBUG

				//最后判断这个窗口是否在这个显示器中
				RECT rct = { 0 };
				if (!::GetWindowRect(hWndCCC, &rct)) continue;

				//全是0的窗口不显示
				if (rct.bottom - rct.top <= 1 || rct.right - rct.left <= 1) continue;

				if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) != hMonitor) continue;

				vec.push_back({ hWndCCC,titleName,className,::GetWindow(hWndCCC, GW_OWNER) });
			}
			return vec;
		};

		auto vecCur = EnumMonitorWnd(hDesktop, hMonitor);

		auto& listWnd = _map_Wnd[hMonitor];

		//如果有窗口在显示
		if (!vecCur.empty())
		{
			//删除list中所有的顶级窗口(非弹出)
			for (auto it = listWnd.begin(); it != listWnd.end();)
			{
				auto cur_it = it++;
				if (!cur_it->hOwerWnd) listWnd.erase(cur_it);
			}

			//最小化所有
			for (auto& rc : vecCur)
			{
				if (!IsWindowVisible(rc.wnd) || IsIconic(rc.wnd)) continue;

				if (rc.hOwerWnd) ShowOwnedPopups(rc.hOwerWnd, false);
				else			 ::ShowWindow(rc.wnd, SW_MINIMIZE);

				console.log("隐藏:{:08X}-{}-{}", (DWORD)rc.wnd, rc.title, rc.cls);
				listWnd.emplace_back(std::move(rc));
			}

			//将桌面置前台取得焦点
			activeWnd(hDesktop);
		}
		else {
			HWND hLastWnd = nullptr;

			for (auto it = listWnd.begin(); it != listWnd.end(); it++)
			{
				auto& rc = *it;
				if (!::IsWindow(rc.wnd)) continue;

				if (rc.hOwerWnd)
				{
 					ShowOwnedPopups(rc.hOwerWnd, true);
				}
				else
				{
					::ShowWindow(rc.wnd, SW_SHOWNOACTIVATE);

					//::SetWindowPos(rc.wnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
					hLastWnd = rc.wnd;
				}

									SetWindowPos(rc.wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
					SetWindowPos(rc.wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE); 

				console.log("显示:{:08X}-{}-{}", (DWORD)rc.wnd, rc.title,rc.cls);
			}

			listWnd.clear();

			activeWnd(hLastWnd);
		}

		return true;
	}

	//窗口信息
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

	//当前监视器矩阵
	static bool getCurrentMonitorRecv(RECT* lpRect)
	{
		auto hMon = getCurrentMonitor();
		auto rct = getMonitorRecv(hMon);
		memcpy(lpRect, &rct, sizeof(RECT));
		return true;
	}

	static RECT getMonitorRecv(HMONITOR hMon)
	{
		RECT rct = { 0 };
		MONITORINFOEX moninfo = { 0 };
		moninfo.cbSize = sizeof(moninfo);

		if (!GetMonitorInfoA(hMon, &moninfo))
		{
			ASSERT(0);
		}
		memcpy(&rct, &moninfo.rcMonitor, sizeof(RECT));
		return rct;
	}

	//当前监视器句柄
	static HMONITOR getCurrentMonitor(POINT *lppos=nullptr)
	{
		if (!lppos)
		{
			POINT pos = { 0 };
			if (!GetCursorPos(&pos)) return nullptr;
			return MonitorFromPoint(pos, MONITOR_DEFAULTTONEAREST);
		}
		else {
			return MonitorFromPoint(*lppos, MONITOR_DEFAULTTONEAREST);
		}
	}

	//当前监视器开始菜单按钮句柄
	static HWND		GetCurrentMonitorStartMenuWnd()
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

		return hWnd;
	}

	//显示开始菜单
	static bool		show_StartMenu()
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
};