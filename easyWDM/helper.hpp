
#include <easy/easy.h>
#include <windows.h>
#include <dwmapi.h>
#include <combaseapi.h>
#include <shldisp.h>

class helper
{
public:

	inline static	HICON	m_runDlgIcon = nullptr;

	static bool ShowRunDlg(bool is_thread = false)
	{
		/*
				//�������߳���ʾ�Ի���
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

				//�����ھ��,ͼ��,����·��,���ڱ���,˵������,δ֪(������ʾΪ0x14��0x4)
		typedef DWORD(WINAPI* LPRUNDLG)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, DWORD);
		static LPRUNDLG s_RunDlg = nullptr;
		if (s_RunDlg == nullptr)
		{
			HMODULE hMod = ::LoadLibrary("shell32.dll");
			if (hMod)s_RunDlg = (LPRUNDLG)::GetProcAddress(hMod, MAKEINTRESOURCE(61));
			if (!s_RunDlg) return false;
		}

		//�������߳���ʾ�Ի���
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
			hWnd = ::FindWindowEx(nullptr, hWnd, "WorkerW", nullptr);
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

	//�жϴ����Ƿ���ʾ��������
	static bool IsToolbarWnd(HWND hWnd)
	{
		DWORD dwExstyle = ::GetWindowLongA(hWnd, GWL_EXSTYLE);
		return dwExstyle & WS_EX_APPWINDOW;
	}

	struct _WND_RC
	{
		HWND wnd = nullptr;
		std::string title;
		std::string cls;
		HWND hOwerWnd = nullptr;
	};

	inline static std::map<HMONITOR, std::vector<_WND_RC>> _map_OwerWnd;

	inline static std::map<HMONITOR, std::vector<_WND_RC>> _map_TopWnd;
	
	//��ʾ��ǰ���ڻ�ǰ��ʾ���ĵ�������
	static bool showOwerWnd(HMONITOR hMonitor,bool is_active)
	{
		auto& vecOwer = _map_OwerWnd[hMonitor];
		HWND hLastWnd = nullptr;
		for (auto& rc : vecOwer)
		{
			if (!::IsWindow(rc.wnd) || IsWindowVisible(rc.wnd)) continue;
			ShowOwnedPopups(rc.hOwerWnd, true);
			hLastWnd = rc.wnd;
		}
		vecOwer.clear();

		if (hLastWnd && is_active) activeWnd(hLastWnd);

		return hLastWnd!=nullptr;
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
					|| className == "WorkerW")
				{
					continue;
				}

				//����ʱ������
			#ifdef _DEBUG
				if (titleName.find("Microsoft Visual Studio(����Ա)") != std::string::npos
					&& titleName.find("(����") != std::string::npos)
				{
					continue;
				}
				//if (className == "HwndWrapper[DefaultDomain;;4da6d4be-d0ca-41a1-b9a9-bf651e51960c]") continue;
			#endif // _DEBUG

				//����ж���������Ƿ��������ʾ����
				RECT rct = { 0 };
				if (!::GetWindowRect(hWndCCC, &rct)) continue;

				//ȫ��0�Ĵ��ڲ���ʾ
				if (rct.bottom - rct.top <= 1 || rct.right - rct.left <= 1) continue;

				if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) != hMonitor) continue;

				vec.push_back({ hWndCCC,titleName,className,::GetWindow(hWndCCC, GW_OWNER) });
			}
			return vec;
		};

		auto vecCur = EnumMonitorWnd(hDesktop, hMonitor);
		
		auto& vecTopWnd = _map_TopWnd[hMonitor];
		auto& vecOWerWnd = _map_OwerWnd[hMonitor];

		//����д�������ʾ
		if (!vecCur.empty())
		{
			vecTopWnd.clear();

			for (auto& rc : vecCur)
			{
				if (!IsWindowVisible(rc.wnd) || IsIconic(rc.wnd)) continue;

				if (rc.hOwerWnd)
				{
					ShowOwnedPopups(rc.hOwerWnd, false);

					vecOWerWnd.emplace_back(std::move(rc));
				}
				else {
					::ShowWindow(rc.wnd, SW_MINIMIZE);		//��С������

					vecTopWnd.emplace_back(std::move(rc));
				}
			}

			//��������ǰ̨ȡ�ý���
			activeWnd(hDesktop);
		}
		else {
			HWND hLastWnd = nullptr;

			//��ԭ����
			for (auto& rc : vecTopWnd)
			{
				if (!::IsWindow(rc.wnd)) continue;
				::ShowWindow(rc.wnd, SW_SHOWNOACTIVATE);
				hLastWnd = rc.wnd;
			}
			
			vecTopWnd.clear();

			if (!showOwerWnd(hMonitor,true))
			{
				activeWnd(hLastWnd);
			}
		}

		return true;
	}

	//������Ϣ
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

	//��ǰ����������
	static bool getCurrentMonitorRecv(RECT* lpRect)
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

	//��ǰ���������
	static HMONITOR getCurrentMonitor()
	{
		POINT pos = { 0 };
		if (!GetCursorPos(&pos))
		{
			return nullptr;
		}
		return MonitorFromPoint(pos, MONITOR_DEFAULTTONEAREST);
	}

	//��ǰ��������ʼ�˵���ť���
	static HWND		GetCurrentMonitorStartMenuWnd()
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

	//��ʾ��ʼ�˵�
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