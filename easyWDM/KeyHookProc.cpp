#include "pch.h"
#include "easyWDM.h"

inline bool is_DownKey(UINT key)
{
    return (GetAsyncKeyState(key) & 0x8000);
}

bool easyWDM::HandlerHotkey(UCHAR ukey, DWORD flags_)
{
    DWORD dwFlags = _KEYS_STATUS::marge_flags(flags_, ukey);

    auto it = _map_hotkey.find(dwFlags);
    if (it == _map_hotkey.end()) return false;
    auto result=it->second();
    return result;
}

bool easyWDM::KeyMessage(UINT uType, KBDLLHOOKSTRUCT* pHook)
{
    if (pHook->dwExtraInfo == 0x3412259)
    {
        console.log("收到标志");
        return false;
    }

    UCHAR uKey = (UCHAR)pHook->vkCode;
    
    //标识是否处理了
    bool isProcessed = false;

    //处理系统key,将他设置为普通key方便处理
    bool is_sys_key = false;
    {
        if (uType == WM_SYSKEYDOWN)
        {
            uType = WM_KEYDOWN;
            is_sys_key = true;
        }
        else if (uType == WM_SYSKEYUP)
        {
            uType = WM_KEYUP;
            is_sys_key = true;
        }
    }

    //记录各个组合键真实状态-省去中间取值环节
    auto down_count = key_status.set_status(uType, uKey);

    //处理热键
    if (uType == WM_KEYDOWN && ((uKey >= '0' && uKey <= '9') || (uKey >= 'A' && uKey <= 'Z') || (uKey >= VK_F1 && uKey <= VK_F24)))
    {
        DWORD hotkey_flags = key_status.get_hotkey_flags();
        
        if (uKey >= VK_F1 && uKey <= VK_F24)
        {
            if (HandlerHotkey(uKey, hotkey_flags))
            {
                isProcessed = true;
            }
        }
        else
        {
            if (hotkey_flags && HandlerHotkey(uKey, hotkey_flags))
            {
                isProcessed = true;
            }
        }
    }

    //过滤win键
    {
        if (uType == WM_KEYDOWN && !isProcessed && m_filter_win_status && uKey != VK_LWIN)
        {
            //console.log("发送按下WIN");
            m_filter_win_status = false;
            keybd_event(VK_LWIN, 0, KEYEVENTF_EXTENDEDKEY | 0, 0x3412259);
            //keybd_event(VK_LWIN, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        }
        
        if (uKey == VK_LWIN)
        {
            //单按下过滤
            if (uType == WM_KEYDOWN && key_status.get_hotkey_flags_noBy(FLAGS_WIN) == 0)
            {
                m_filter_win_status = true;
                isProcessed = true;
                //console.log("过滤WIN");
            }

            //弹起重置标志
            if (uType == WM_KEYUP && m_filter_win_status)
            {
                //console.log("重置WIN");
                m_filter_win_status = false;
                isProcessed = true;
            }
        }
    }

    //单按键热键
    struct _hotkey_single
    {
        _hotkey_single(UINT key, UINT flags)
        {
            listenKey = key;
            listenFlags = flags;
        }

        inline bool is_event(UINT uType, UINT key)
        {
            if (key != listenKey) return false;

            if (uType == WM_KEYDOWN)
            {
                //如果是第一次按下,且没有按下其它控制键
                if (last_tick == 0 && key_status.get_hotkey_flags_noBy(listenFlags) == 0)
                {
                    last_tick = key_status._last_down_tick;
                }
                return false;
            }
            else if ((uType == WM_KEYUP) && last_tick)
            {
                bool go_event = false;
                //间隔时间小于200ms,且期间没有按下过其它按键
                if (::GetTickCount64() - last_tick < 200 && last_tick == key_status._last_down_tick) go_event = true;
                last_tick = 0;
                return go_event;
            }
            return false;
        }

        UINT listenKey = 0;
        UINT listenFlags = 0;
        uint64_t last_tick = 0;
    };

    static _hotkey_single _single_win(VK_LWIN, FLAGS_WIN);
    static _hotkey_single _single_alt(VK_LMENU, FLAGS_ALT);
    static _hotkey_single _single_shift(VK_LSHIFT, FLAGS_SHIFT);
    static _hotkey_single _single_ctrl(VK_LCONTROL, FLAGS_CTRL);

    if (_single_win.is_event(uType, uKey)) HandlerHotkey(0, _single_win.listenFlags);
    else if (_single_alt.is_event(uType, uKey)) HandlerHotkey(0, _single_alt.listenFlags);
    else if (_single_shift.is_event(uType, uKey)) HandlerHotkey(0, _single_shift.listenFlags);
    else if (_single_ctrl.is_event(uType, uKey)) HandlerHotkey(0, _single_ctrl.listenFlags);

    //处理系统按键的双击事件
    {
        //最后一次系统按键与弹起时间
        static UCHAR s_last_key = 0;
        static uint64_t s_last_up_tick = 0;
        if (uKey == VK_LWIN
            || uKey == VK_RWIN)
        {
        }
        if (s_last_key != uKey)
        {
            s_last_key = uKey;
            s_last_up_tick = ::GetTickCount64();
        }
    }

    return isProcessed;
}

LRESULT CALLBACK easyWDM::KeyHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
    {
        return CallNextHookEx(_hookKey, nCode, wParam, lParam);
    }

    if (nCode == HC_ACTION && m_pThis->KeyMessage((UINT)wParam, (KBDLLHOOKSTRUCT*)lParam))
    {
        return true;
    }
    else {
        return CallNextHookEx(_hookKey, nCode, wParam, lParam);
    }
}
