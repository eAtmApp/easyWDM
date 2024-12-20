
#include <windows.h>

#include <oleacc.h>
#pragma comment(lib, "Oleacc.lib")

#include <dwmapi.h>
#include <combaseapi.h>
#include <shldisp.h>
#include <tlhelp32.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")
#include <iostream>
#include <easylib/format_type.h>

#include "easy_uiautomation.hpp"

#define SHOW_DEBUG_LOG 0

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
typedef etd::map<HMONITOR, MONITOR_INFO> MONITOR_INFOS;

class helper
{
public:

    inline static	HICON	m_runDlgIcon = nullptr;

    //得到当前监视器任务栏句柄
    static HWND getCurrentMonitorTaskBarWnd()
    {
        //当前监视器句柄
        auto hMonitor = helper::getCurrentMonitor();

        //先得到进程句柄
        HWND hWnd = ::FindWindowA("Shell_TrayWnd", nullptr);
        if (hWnd)
        {
            RECT rct = {0};
            ::GetWindowRect(hWnd, &rct);
            if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) == hMonitor) return hWnd;
        }

        hWnd = ::FindWindowA("Shell_SecondaryTrayWnd", nullptr);

        while (hWnd)
        {
            RECT rct = {0};
            ::GetWindowRect(hWnd, &rct);
            if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) == hMonitor) break;
            hWnd = ::FindWindowExA(nullptr, hWnd, "Shell_SecondaryTrayWnd", nullptr);
        }
        return hWnd;
    }

    static eString getProcessName(DWORD dwProcessId)
    {
        eString result;
        HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapShot)
        {
            PROCESSENTRY32 pe32 = {0};
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

    static DWORD	getProcessId(HWND hWnd)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        return pid;
    }

    static eString getProcessName(HWND hWnd)
    {
        auto pid = getProcessId(hWnd);
        if (pid) return getProcessName(pid);
        return "";
    }

    //显示运行对话框
    static bool ShowRunDlg(bool async = false)
    {
        //父窗口句柄,图标,工作路径,窗口标题,说明文字,未知(跟踪显示为0x14或0x4)
        typedef DWORD(WINAPI* LPRUNDLG)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, DWORD);
        static LPRUNDLG s_RunDlg = nullptr;
        if (s_RunDlg == nullptr)
        {
            HMODULE hMod = ::LoadLibrary("shell32.dll");
            if (hMod)s_RunDlg = (LPRUNDLG)::GetProcAddress(hMod, MAKEINTRESOURCE(61));
            if (!s_RunDlg)
            {
                console.out_api_error("GetProcAddress(#64)");
                return false;
            }
        }

        //不在主线程显示对话框
        if (async)
        {
            worker.async(helper::ShowRunDlg, false);
            return true;
        }

        RECT rct = {0};
        getCurrentMonitorRecv(&rct);

        PVOID OldValue = nullptr;
        Wow64DisableWow64FsRedirection(&OldValue);

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
                                //error_code = runApp2(path);
                                if (!runApp("open", path, "", "", -1, &error_code))
                                {
                                    eString err_type = process.get_last_error_message(error_code);

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

        if (OldValue) Wow64RevertWow64FsRedirection(OldValue);

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
        #define MIN_SIZE_WND 100*100    //最小只处理像素面积

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
                RECT rct = {0};
                if (!::GetWindowRect(hWndCCC, &rct)) continue;

                if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) != hMonitor) continue;

                //像素面积小于MIN_SIZE_WND的不处理
                if ((rct.bottom - rct.top) * (rct.right - rct.left) < MIN_SIZE_WND) continue;

                if ((rct.bottom - rct.top) < 50 || (rct.right - rct.left) < 50) continue;

                vec.push_back({hWndCCC,titleName,className,::GetWindow(hWndCCC, GW_OWNER)});
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

                #if SHOW_DEBUG_LOG==1
                console.log("显示:{:08X}-{}-{}", (DWORD)rc.wnd, rc.title, rc.cls);
                #endif // SHOW_WND_LOG==1
            }

            listWnd.clear();

            activeWnd(hLastWnd);
        }

        return true;
    }

    static	bool	runApp(etd::string type, FilePath path, etd::string param, etd::string dir, int showtype = -1, int* lpErrCode = nullptr)
    {
        path.trim();

        bool bfoundFile = false;

        PVOID OldValue = nullptr;
        Wow64DisableWow64FsRedirection(&OldValue);

        if (path.is_absolute())
        {
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
                char szBuffer[1024] = {0};
                GetEnvironmentVariableA("USERPROFILE", szBuffer, sizeof(szBuffer));
                dir = szBuffer;
            }
        }

        SHELLEXECUTEINFOA execInfo = {0};
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

        if (lpErrCode)
        {
            *lpErrCode = (int)execInfo.hInstApp;
        }

        if (OldValue) Wow64RevertWow64FsRedirection(OldValue);

        return (int)execInfo.hInstApp > 32;
    }

    static  int    runApp2(eString cmdline, eString dir = "")
    {
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi{0};
        si.cb = sizeof(STARTUPINFOA);

        if (!CreateProcessA(NULL, (LPSTR)cmdline.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            auto err_code = ::GetLastError();
            auto msg = process.get_last_error_message(err_code);

            return err_code;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return 0;
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
        if (!hMon) return false;

        auto rct = getMonitorRecv(hMon);
        memcpy(lpRect, &rct, sizeof(RECT));
        return true;
    }

    static RECT getMonitorRecv(HMONITOR hMon)
    {
        ASSERT(hMon != nullptr);
        RECT rct = {0};
        MONITORINFOEX moninfo = {0};
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
        POINT pos = {0};

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
    static HWND		GetCurMonitorTaskBarButton(LPCTSTR szChildClass)
    {
        HWND hResult = nullptr;

        //查找所有开始菜单按钮
        auto _findStart = [=](std::vector<HWND>& vec, const char* clsName)
        {
            HWND hLastWnd = nullptr;
            do
            {
                hLastWnd = FindWindowExA(nullptr, hLastWnd, clsName, nullptr);
                if (hLastWnd)
                {
                    HWND hWnd = ::FindWindowExA(hLastWnd, nullptr, szChildClass, nullptr);
                    std::string ts(szChildClass);
                    if (ts == "ReBarWindow32")
                    {
                        hWnd = ::FindWindowExA(hWnd, nullptr, nullptr, nullptr);
                        hWnd = ::FindWindowExA(hWnd, nullptr, nullptr, nullptr);
                    }

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
                hResult = wnd;
                break;
            }
        }

        return hResult;
    }

    static void pressEscKey() {
        // 模拟按下 ESC 键
        keybd_event(VK_ESCAPE, 0, 0, 0); // 按下 ESC 键

        ::Sleep(10);

        // 模拟释放 ESC 键
        keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0); // 释放 ESC 键
    }

    //调用热键
    static void call_hotkey(const char* args)
    {
        static etd::map<etd::string, BYTE> keyMap = {
            {"backspace", VK_BACK},
            {"tab", VK_TAB},
            {"enter", VK_RETURN},
            {"shift", VK_SHIFT},
            {"ctrl", VK_CONTROL},
            {"alt", VK_MENU},
            {"pause", VK_PAUSE},
            {"capslock", VK_CAPITAL},
            {"escape", VK_ESCAPE},
            {"space", VK_SPACE},
            {"pageup", VK_PRIOR},
            {"pagedown", VK_NEXT},
            {"end", VK_END},
            {"home", VK_HOME},
            {"left", VK_LEFT},
            {"up", VK_UP},
            {"right", VK_RIGHT},
            {"down", VK_DOWN},
            {"insert", VK_INSERT},
            {"delete", VK_DELETE},
            {"del", VK_DELETE},
            {"f1", VK_F1},
            {"f2", VK_F2},
            {"f3", VK_F3},
            {"f4", VK_F4},
            {"f5", VK_F5},
            {"f6", VK_F6},
            {"f7", VK_F7},
            {"f8", VK_F8},
            {"f9", VK_F9},
            {"f10", VK_F10},
            {"f11", VK_F11},
            {"f12", VK_F12},
            {"numlock", VK_NUMLOCK},
            {"scrolllock", VK_SCROLL},
            {"printscreen", VK_SNAPSHOT},
            {"win", VK_LWIN},  // 左侧 Windows 键
            {"apps", VK_APPS}, // 应用程序键
        };

        eString keys(args);
        keys = keys.tolower();
        auto arr = keys.split_string("+");
        etd::vector<BYTE> keysCode;

        bool is_error = false;

        arr.forEach([&](eString& keyStr)
        {
            if (keyStr.size() == 1)
            {
                keyStr = keyStr.toupper();
                BYTE bc = keyStr[0];
                keysCode.push_back(bc);
            }
            else {
                BYTE bc = 0;
                if (keyMap.Lookup(keyStr, bc))
                {
                    keysCode.push_back(bc);
                }
                else {
                    box.ShowInfo("不识别键盘信息:{}", keyStr);
                    is_error = true;
                }
            }
        });

        keysCode.forEach([](BYTE& b)
        {
            keybd_event(b, 0, 0, 0x3412259); // 按下 ESC 键
            Sleep(10);
        });

        keysCode.forEach([](BYTE& b)
        {
            keybd_event(b, 0, KEYEVENTF_KEYUP, 0x3412259); // 释放 ESC 键
            Sleep(10);
        });
    }

    //StartButton
    //TaskViewButton
    static void TaskBar_Button(const char* uiId, bool async = true)
    {
        if (async)
        {
            worker.async(TaskBar_Button, uiId, false);
            return;
        }

        //得到当前监视器/按钮对象
        //用静态map存储对象
        auto get_element_object = [&]() -> easy_uiautomation&
        {
            static etd::map<eString, easy_uiautomation> mapUiAuto;
            eString csMapId;
            auto hMonitor = getCurrentMonitor();
            csMapId.Format("{}_{}", uiId, (DWORD)hMonitor);

            auto& element = mapUiAuto[csMapId];

            if (!element)
            {
                console.time();
                auto hTaskBar = helper::getCurrentMonitorTaskBarWnd();
                easy_uiautomation uiAuto(hTaskBar);
                if (uiAuto)
                {
                    uiAuto.AddCond(UIA_AutomationIdPropertyId, etd::string::ansi_unicode(uiId).c_str());
                    element = uiAuto.FindElement();
                }
                #if SHOW_DEBUG_LOG==1
                console.timeEnd();
                #endif // SHOW_DEBUG_LOG==1

                if (!element)
                {
                    console.error("获取新对象失败");
                }
                else {
                    console.log("获取新对象成功");
                }
            }
            else {
                console.log("已有对象");
            }
            return element;
        };

        auto& buttonElement = get_element_object();
        if (buttonElement)
        {
            if (buttonElement.ToggleState())
            {
                console.log("ToggleState=true");
                pressEscKey();
            }
            else {

                auto hTaskBar = helper::getCurrentMonitorTaskBarWnd();
                HWND hForeWnd;
                DWORD dwForeID;
                DWORD dwCurID;
                hForeWnd = GetForegroundWindow();
                dwCurID = GetCurrentThreadId();
                //dwForeID = GetWindowThreadProcessId(hForeWnd, nullptr);
                dwForeID = GetWindowThreadProcessId(hTaskBar, nullptr);
                AttachThreadInput(dwCurID, dwForeID, TRUE);
                ShowWindow(hTaskBar, SW_SHOWNORMAL);

                console.time();
                if (!buttonElement.ClickElement())
                {
                    buttonElement.Clear();
                }
                #if SHOW_DEBUG_LOG==1
                console.timeEnd();
                #endif // SHOW_DEBUG_LOG==1

                SetForegroundWindow(hTaskBar);
                AttachThreadInput(dwCurID, dwForeID, FALSE);
            }
        }

        /*
                auto hTaskBar = helper::getCurrentMonitorTaskBarWnd();
                if (hTaskBar)
                {
                    console.time(uiId);
                    easy_uiautomation uiAuto(hTaskBar);
                    if (uiAuto)
                    {
                        uiAuto.AddCond(UIA_AutomationIdPropertyId, etd::string::ansi_unicode(uiId).c_str());
                        auto but = uiAuto.FindElement();
                        if (!but) return;
                        if (!but.ToggleState())
                        {
                            auto ret=but.ClickElement();
                            console.log("click_{}", ret);
                        }
                        else {
                            pressEscKey();
                        }
                    }
                    console.timeEnd(uiId);
                }*/
    }

    static void	_show_start_bak(bool async = true)
    {
        TaskBar_Button("StartButton", async);
    }
    static void	show_taskview(bool async = true)
    {
        TaskBar_Button("TaskViewButton", async);
    }

    //枚举显示器
    static int enumMonitor(MONITOR_INFOS& infos)
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

                DEVMODEA devmode = {0};

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


    //当前监视器开始菜单按钮句柄
    static HWND		GetCurrentMonitorStartMenuWnd()
    {
        HWND hResult = nullptr;

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
                hResult = wnd;
                break;
            }
        }
        return hResult;
    }

    //显示开始菜单
    static void show_start(bool async = true)
    {
        if (async)
        {
            worker.async(show_start, false);
            return;
        }

        console.time();

        HWND hStartWnd = GetCurrentMonitorStartMenuWnd();
        if (!hStartWnd) return;

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

        //UIAutoInvoke(hStartWnd);

        easy_uiautomation ui(hStartWnd);
        ui.ClickElement();

        #if SHOW_DEBUG_LOG==1
        console.timeEnd();
        #endif // SHOW_DEBUG_LOG==1

        //std::thread thread(thread_proc);
        //thread.detach();
        //return true;
    }
};