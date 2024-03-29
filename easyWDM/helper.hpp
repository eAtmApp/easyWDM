
#include <windows.h>
#include <dwmapi.h>
#include <combaseapi.h>
#include <shldisp.h>
#include <tlhelp32.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#include <easylib/format_type.h>

using namespace easy;

struct MONITOR_INFO
{
	HMONITOR hMonitor; //显示器句柄
	TCHAR szDevice[32]; //显示器名
	RECT rcVirtual; //虚拟显示屏坐标
	RECT rcMonitor; //物理显示坐标
	RECT rcWork; //工作显示坐标
	BOOL bPrimary; //主显示器？

	int  mmX;		//物理尺寸宽度
	int	 mmY;		//物理尺寸高度mm为单位

	MONITOR_INFO()
	{
		memset(this, 0, sizeof(*this));
	}
};
typedef etd::map<HMONITOR,MONITOR_INFO> MONITOR_INFOS;

class helper
{
public:

	inline static	HICON	m_runDlgIcon = nullptr;

	static eString getProcessName(DWORD dwProcessId)
	{
		eString result;
		HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapShot)
		{
			PROCESSENTRY32 pe32 = { 0 };
			pe32.dwSize = sizeof(PROCESSENTRY32);
			if (::Process32First(hSnapShot, &pe32))
			{
				do
				{
					if (dwProcessId == pe32.th32ProcessID)
					{
						result = pe32.szExeFile;
						break;
					}
				} while (::Process32Next(hSnapShot, &pe32));
			}
			CloseHandle(hSnapShot);
		}
		return result;
	}

	static eString getProcessName(HWND hWnd)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(hWnd, &pid);
		if (pid) return getProcessName(pid);
		return "";
	}

	static bool ShowRunDlg(bool is_thread = false)
	{
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
			static WNDPROC prevWndProc;

			struct _tagTemp {
				static LRESULT CALLBACK WndProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
					switch (uMsg) {
					case WM_NOTIFY:
					{
						if (wParam == 0)
						{
							struct tagNMRUNFILEDLGW {
								NMHDR		hdr;
								LPCWSTR		lpszFile;
								LPCWSTR		lpszDirectory;
								int			nShow;
							};
							auto param = (tagNMRUNFILEDLGW*)lParam;
							eString path = util::unicode_ansi(param->lpszFile);

							int error_code = 0;
							if (!runApp("open", path, "", "", -1, &error_code))
							{
								eString err_type;
								switch (error_code)
								{
								case SE_ERR_FNF:
									err_type = "文件未找到";
									break;
								case SE_ERR_PNF:
									err_type = "找不到路径";
									break;
								case SE_ERR_ACCESSDENIED:
									err_type = "拒绝访问";
									break;
								case SE_ERR_OOM:
									err_type = "内存溢出";
									break;
								case SE_ERR_DLLNOTFOUND:
									err_type = "动态链接库未找到";
									break;
								case SE_ERR_SHARE:
									err_type = "共享文件未找到";
									break;
								case SE_ERR_ASSOCINCOMPLETE:
									err_type = "文件关联信息不完整";
									break;
								case SE_ERR_DDETIMEOUT:
									err_type = "DDE操作超时";
									break;
								case SE_ERR_DDEFAIL:
									err_type = "DDE操作失败";
									break;
								case SE_ERR_DDEBUSY:
									err_type = "DDE正忙";
									break;
								case SE_ERR_NOASSOC:
									err_type = "没有找到关联的应用程序";
									break;
								default:
									err_type.Format("操作失败({})", error_code);
								}

								eString err_msg;
								err_msg.Format("找不开文件 '{}' {}", path.c_str(), err_type.c_str());
								::MessageBoxA(hwnd, err_msg.c_str(), path.c_str(), MB_OK | MB_ICONERROR);
								return 2;
							}
							else {
								return 1;
							}
						}
					}
					default:
						return CallWindowProc(prevWndProc, hwnd, uMsg, wParam, lParam);
					}
					return 0;
				};
			};
			prevWndProc = (WNDPROC)SetWindowLongPtr(hSBWnd, GWLP_WNDPROC, (LONG_PTR)&_tagTemp::WndProcedure);

			activeWnd(hSBWnd);
			s_RunDlg(hSBWnd, nullptr, nullptr, nullptr, nullptr, 0x4 | 0x1);
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
		EASY_STATIC_LOCK();

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
					|| className == "SysShadow"
					|| className == "TaskListThumbnailWnd")
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

				console.log("隐藏:{:x}-{}-{}", (uint64_t)rc.wnd, rc.title, rc.cls);
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

				console.log("显示:{:08X}-{}-{}", (DWORD)rc.wnd, rc.title, rc.cls);
			}

			listWnd.clear();

			activeWnd(hLastWnd);
		}

		return true;
	}

	static	bool	runApp(etd::string type, FilePath path, etd::string param, etd::string dir, int showtype = -1, int* lpErrCode = nullptr)
	{
		PVOID OldValue = nullptr;
		BOOL bDisableWow64 = FALSE;

		path.trim();

		bool bfoundFile = false;

		if (!path.is_absolute())
		{
			bDisableWow64 = Wow64DisableWow64FsRedirection(&OldValue);
		}
		else {
			//从绝对路径判断文件.
			bfoundFile = path.is_exists();
		}

		//处理路径中的空格
		if (!bfoundFile && type == "open" && param.empty() &&
			(path.find(" ") != std::string::npos || path.find("　") != std::string::npos))
		{
			bool isYingHao = false;
			for (size_t i = 0; i < path.size(); i++)
			{
				auto c = path[i];
				if (c == '\"')
				{
					isYingHao = !isYingHao;
					continue;
				}

				if (c == ' ' || c == '　')
				{
					auto tmp = path;
					path = tmp.substr(0, i);
					param = tmp.substr(i + 1);
					path.trim();
					param.trim();
					break;
				}
			}
		}

		if (showtype == -1) showtype = SW_SHOWDEFAULT;

		if (dir.empty())
		{
			if (path.is_absolute()) dir = path.to_file_dir();
			else {
				char szBuffer[1024] = { 0 };
				GetEnvironmentVariableA("USERPROFILE", szBuffer, sizeof(szBuffer));
				dir = szBuffer;
			}
		}

		SHELLEXECUTEINFOA execInfo = { 0 };
		execInfo.cbSize = sizeof(execInfo);
		execInfo.lpVerb = "open";
		execInfo.lpFile = path.c_str();
		if (!param.empty()) execInfo.lpParameters = param.c_str();
		if (!dir.empty()) execInfo.lpDirectory = dir.c_str();
		execInfo.nShow = showtype;

		execInfo.fMask = SEE_MASK_FLAG_NO_UI;
		execInfo.fMask |= SEE_MASK_HMONITOR;

		execInfo.hMonitor = getCurrentMonitor();

		bool result = ShellExecuteExA(&execInfo);
		
		if (bDisableWow64) Wow64RevertWow64FsRedirection(OldValue);

		if (lpErrCode)
		{
			*lpErrCode = (int)execInfo.hInstApp;
		}
		return (int)execInfo.hInstApp > 32;
	}

	/*
		static	bool	runApp(etd::string type, etd::string path, etd::string param, etd::string dir)
		{
			PVOID OldValue = nullptr;
			BOOL bDisableWow64 = FALSE;

			if (!path.is_full_path())
			{
				bDisableWow64 = Wow64DisableWow64FsRedirection(&OldValue);
			}

			if (dir.empty())
			{
				if (path.is_full_path()) dir = path.to_file_dir();
				else {
					//dir = "XXX";
					char szBuffer[1024] = { 0 };
					GetEnvironmentVariableA("USERPROFILE", szBuffer, sizeof(szBuffer));
					dir = szBuffer;
				}
			}

			auto result = ShellExecuteA(nullptr,
				type.empty() ? nullptr : type.data(),
				path.data(),
				param.empty() ? nullptr : param.data(),
				dir.empty() ? nullptr : dir.data(),
				SW_SHOWNORMAL);

			if (bDisableWow64) Wow64RevertWow64FsRedirection(OldValue);

			return result > (HINSTANCE)32;
		}*/

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
		if (!hMon) return false;

		auto rct = getMonitorRecv(hMon);
		memcpy(lpRect, &rct, sizeof(RECT));
		return true;
	}

	static RECT getMonitorRecv(HMONITOR hMon)
	{
		ASSERT(hMon != nullptr);
		RECT rct = { 0 };
		MONITORINFOEX moninfo = { 0 };
		moninfo.cbSize = sizeof(moninfo);

		if (!GetMonitorInfoA(hMon, &moninfo))
		{
			console.out_api_error("MonitorFromPoint");
			return rct;
		}
		memcpy(&rct, &moninfo.rcMonitor, sizeof(RECT));
		return rct;
	}

	//当前监视器句柄
	static HMONITOR getCurrentMonitor(POINT* lppos = nullptr)
	{
		POINT pos = { 0 };

		if (!lppos)
		{
			if (!GetCursorPos(&pos))
			{
				console.out_api_error("GetCursorPos");
				return nullptr;
			}
			lppos = &pos;
		}

		auto ret = MonitorFromPoint(*lppos, MONITOR_DEFAULTTONEAREST);
		if (ret == nullptr)
		{
			console.out_api_error("MonitorFromPoint");
			return nullptr;
		}
		return ret;
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

		auto thread_proc = []() {
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
			
			PostMessageA(hStartWnd, WM_MOUSEMOVE, 0, MAKELONG(5, 5));
			PostMessageA(hStartWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELONG(5, 5));
			PostMessageA(hStartWnd, WM_LBUTTONUP, 0, MAKELONG(5, 5));

			return false;
		};

		std::thread thread(thread_proc);
		thread.detach();
		return true;
	}

	//枚举显示器
	static int enumMonitor(MONITOR_INFOS &infos)
	{
		//输出显示器信息
		auto MonitorEnumProc = [](
			HMONITOR hMonitor,  // handle to display monitor
			HDC hdcMonitor,     // handle to monitor-appropriate device context
			LPRECT lprcMonitor, // pointer to monitor intersection rectangle
			LPARAM dwData       // data passed from EnumDisplayMonitors
			)
		{
			MONITOR_INFOS& infos = *((MONITOR_INFOS*)dwData);

			// GetMonitorInfo 获取显示器信息
			MONITORINFOEX infoEx;
			memset(&infoEx, 0, sizeof(infoEx));
			infoEx.cbSize = sizeof(infoEx);
			if (GetMonitorInfo(hMonitor, &infoEx))
			{
				//保存显示器信息
				MONITOR_INFO info;

				MONITOR_INFO* pInfo = &info;
				pInfo->hMonitor = hMonitor;
				if (lprcMonitor)
				{
					pInfo->rcVirtual = *lprcMonitor;
				}
				pInfo->rcMonitor = infoEx.rcMonitor;
				pInfo->rcWork = infoEx.rcWork;
				pInfo->bPrimary = infoEx.dwFlags == MONITORINFOF_PRIMARY;
				_tcscpy_s(pInfo->szDevice, infoEx.szDevice);
				
				DEVMODEA devmode = { 0 };

				HDC hdc = CreateDCA(pInfo->szDevice, NULL, NULL, NULL);		//得到整个屏幕的设备环境句柄
				pInfo->mmX = GetDeviceCaps(hdc, HORZSIZE);
				pInfo->mmY = GetDeviceCaps(hdc, VERTSIZE);

				/*
				#define MM_TO_INCH (double) 0.0393700788
				double cz = sqrt(cx * cx + cy * cy) * MM_TO_INCH;
				*/

				DeleteDC(hdc);
				
				infos[hMonitor] = info;
			}
			
			return TRUE;
		};

		EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorEnumProc, (LPARAM)&infos);
		return (int)infos.size();
	}
};