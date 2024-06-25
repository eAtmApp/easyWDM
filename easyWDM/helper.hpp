
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

#include <UIAutomation.h>

#include <easylib/format_type.h>

using namespace easy;

struct MONITOR_INFO
{
    HMONITOR hMonitor; //��ʾ�����
    TCHAR szDevice[32]; //��ʾ����
    RECT rcVirtual; //������ʾ������
    RECT rcMonitor; //������ʾ����
    RECT rcWork; //������ʾ����
    BOOL bPrimary; //����ʾ����

    int  mmX;		//����ߴ���
    int	 mmY;		//����ߴ�߶�mmΪ��λ

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

    /*
        eString GetProcessCommandLine(DWORD processId)
        {
            eString commandLine;
            HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (processHandle == NULL)
            {
                return commandLine;
            }

            DWORD bufferSize = 1024;
            LPWSTR buffer = new WCHAR[bufferSize];

            if (GetCommandLineW(processHandle, buffer, bufferSize))
            {
                commandLine.push_back(std::string(buffer, bufferSize));
            }
            else
            {
                std::cerr << "Failed to get command line for process with ID " << processId << std::endl;
            }

            delete[] buffer;

            CloseHandle(processHandle);

            return commandLine;
        }*/

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

    //��ʾ���жԻ���
    static bool ShowRunDlg(bool async = false)
    {
        //�����ھ��,ͼ��,����·��,���ڱ���,˵������,δ֪(������ʾΪ0x14��0x4)
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

        //�������߳���ʾ�Ի���
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
                                if (!runApp("open", path, "", "", -1, &error_code))
                                //error_code = runApp2(path);
                                if (error_code)
                                {
                                    eString err_type = process.get_last_error_message(error_code);

                                    eString err_msg;
                                    err_msg.Format("�Ҳ����ļ� '{}' {}", path.c_str(), err_type.c_str());
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

    //��ʾ��ǰ���ڻ�ǰ��ʾ���ĵ�������
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
        #define MIN_SIZE_WND 100*100    //��Сֻ�����������

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

                //����ʱ������
                #ifdef _DEBUG
                if (titleName.find("Microsoft Visual Studio(����Ա)") != std::string::npos
                    && titleName.find("(����") != std::string::npos)
                {
                    //continue;
                }
                //if (className == "HwndWrapper[DefaultDomain;;4da6d4be-d0ca-41a1-b9a9-bf651e51960c]") continue;
                #endif // _DEBUG

                    //����ж���������Ƿ��������ʾ����
                RECT rct = {0};
                if (!::GetWindowRect(hWndCCC, &rct)) continue;

                if (MonitorFromRect(&rct, MONITOR_DEFAULTTONEAREST) != hMonitor) continue;

                //�������С��MIN_SIZE_WND�Ĳ�����
                if ((rct.bottom - rct.top) * (rct.right - rct.left) < MIN_SIZE_WND) continue;

                if ((rct.bottom - rct.top) < 50 || (rct.right - rct.left) < 50) continue;

                vec.push_back({hWndCCC,titleName,className,::GetWindow(hWndCCC, GW_OWNER)});
            }
            return vec;
        };

        auto vecCur = EnumMonitorWnd(hDesktop, hMonitor);

        auto& listWnd = _map_Wnd[hMonitor];

        //����д�������ʾ
        if (!vecCur.empty())
        {
            //ɾ��list�����еĶ�������(�ǵ���)
            for (auto it = listWnd.begin(); it != listWnd.end();)
            {
                auto cur_it = it++;
                if (!cur_it->hOwerWnd) listWnd.erase(cur_it);
            }

            //��С������
            for (auto& rc : vecCur)
            {
                if (!IsWindowVisible(rc.wnd) || IsIconic(rc.wnd)) continue;

                if (rc.hOwerWnd) ShowOwnedPopups(rc.hOwerWnd, false);
                else			 ::ShowWindow(rc.wnd, SW_MINIMIZE);

                console.log("����:{:x}-{}-{}", (uint64_t)rc.wnd, rc.title, rc.cls);
                listWnd.emplace_back(std::move(rc));
            }

            //��������ǰ̨ȡ�ý���
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

                console.log("��ʾ:{:08X}-{}-{}", (DWORD)rc.wnd, rc.title, rc.cls);
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
            //�Ӿ���·���ж��ļ�.
            bfoundFile = path.is_exists();
        }

        //����·���еĿո�
        if (!bfoundFile && type == "open" && param.empty() &&
            (path.find(" ") != std::string::npos || path.find("��") != std::string::npos))
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

                if (c == ' ' || c == '��')
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
            auto msg=process.get_last_error_message(err_code);
            
            return err_code;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return 0;
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

    //��ǰ���������
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

    //��ǰ��������ʼ�˵���ť���
    static HWND		GetCurrentMonitorStartMenuWnd()
    {
        HWND hResult = nullptr;

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
                hResult = wnd;
                break;
            }
        }

        return hResult;
    }

    static bool UIAutoInvoke(HWND hwnd)
    {
        IUIAutomation* sp_utomation = nullptr;
        IUIAutomationElement* sp_hwnd_element = nullptr;
        IUIAutomationInvokePattern* pToggle = nullptr;

        HRESULT hr = S_FALSE;

        bool result = false;

        do
        {
            hr = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation, reinterpret_cast<void**>(&sp_utomation));
            if (FAILED(hr) || sp_utomation == nullptr) break;

            hr = sp_utomation->ElementFromHandle(hwnd, &sp_hwnd_element);
            if (FAILED(hr) || sp_hwnd_element == nullptr) break;

            hr = sp_hwnd_element->GetCurrentPattern(UIA_InvokePatternId, reinterpret_cast<IUnknown**>(&pToggle));
            if (FAILED(hr) || pToggle == nullptr) break;

            result = pToggle->Invoke() == S_OK;

        } while (false);
        console.log("UIAuto Invoke:{}", result);

        if (pToggle) pToggle->Release();
        if (sp_hwnd_element) sp_hwnd_element->Release();
        if (sp_utomation) sp_utomation->Release();

        return result;
    }

    //��ʾ��ʼ�˵�
    static void show_StartMenu(bool async = true)
    {
        if (async)
        {
            worker.async(show_StartMenu, false);
            return;
        }

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

        UIAutoInvoke(hStartWnd);
        
        //std::thread thread(thread_proc);
        //thread.detach();
        //return true;
    }

    //ö����ʾ��
    static int enumMonitor(MONITOR_INFOS& infos)
    {
        //�����ʾ����Ϣ
        auto MonitorEnumProc = [](
            HMONITOR hMonitor,  // handle to display monitor
            HDC hdcMonitor,     // handle to monitor-appropriate device context
            LPRECT lprcMonitor, // pointer to monitor intersection rectangle
            LPARAM dwData       // data passed from EnumDisplayMonitors
            )
        {
            MONITOR_INFOS& infos = *((MONITOR_INFOS*)dwData);

            // GetMonitorInfo ��ȡ��ʾ����Ϣ
            MONITORINFOEX infoEx;
            memset(&infoEx, 0, sizeof(infoEx));
            infoEx.cbSize = sizeof(infoEx);
            if (GetMonitorInfo(hMonitor, &infoEx))
            {
                //������ʾ����Ϣ
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

                HDC hdc = CreateDCA(pInfo->szDevice, NULL, NULL, NULL);		//�õ�������Ļ���豸�������
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