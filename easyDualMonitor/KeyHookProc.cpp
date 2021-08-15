#include "easyWDM.h"
#include "helper.hpp"

inline bool is_DownKey(UINT key)
{
	return (GetAsyncKeyState(key) & 0x8000);
}

bool easyWDM::HandlerHotkey(UCHAR ukey, DWORD flags_)
{
	DWORD dwFlags = _KEYS_STATUS::marge_flags(flags_, ukey);

	auto it = _map_hotkey.find(dwFlags);
	if (it == _map_hotkey.end()) return false;
	return it->second();
	return false;
}

bool easyWDM::KeyMessage(UINT uType, KBDLLHOOKSTRUCT* pHook)
{
	if (pHook->dwExtraInfo == 0x3412259) return false;

	UCHAR uKey = (UCHAR)pHook->vkCode;

	bool is_handler = false;

	//����ϵͳkey,��������Ϊ��ͨkey���㴦��
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

	//��¼������ϼ���ʵ״̬-ʡȥ�м�ȡֵ����
	auto down_count = key_status.set_status(uType, uKey);

	//�����ȼ�
	if (uType == WM_KEYDOWN && ((uKey >= '0' && uKey <= '9') || (uKey >= 'A' && uKey <= 'Z') || (uKey >= VK_F1 && uKey <= VK_F24)))
	{
		DWORD flags = key_status.get_hotkey_flags();
		if (uKey >= VK_F1 && uKey <= VK_F24)
		{
			if (HandlerHotkey(uKey, flags))
			{
				is_handler = true;
			}
		}
		else
		{
			if (flags && HandlerHotkey(uKey, flags))
			{
				is_handler = true;
			}
		}
	}

	//����win��
	{
		static	bool	_is_filter_win = false;

		if (uType == WM_KEYDOWN && !is_handler && _is_filter_win && uKey != VK_LWIN)
		{
			console.debug("��ԭWIN");
			_is_filter_win = false;
			keybd_event(VK_LWIN, 0, KEYEVENTF_EXTENDEDKEY | 0, 0x3412259);
			//keybd_event(VK_LWIN, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}

		if (uKey == VK_LWIN)
		{
			//�����¹���
			if (uType == WM_KEYDOWN && key_status.get_hotkey_flags_noBy(FLAGS_WIN) == 0)
			{
				_is_filter_win = true;
				is_handler = true;
				console.log("����WIN");
			}

			//�������ñ�־
			if (uType == WM_KEYUP && _is_filter_win)
			{
				console.log("����WIN");
				_is_filter_win = false;
				is_handler = true;
			}
		}
	}

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
				//����ǵ�һ�ΰ���,��û�а����������Ƽ�
				if (last_tick == 0 && key_status.get_hotkey_flags_noBy(listenFlags) == 0)
				{
					last_tick = key_status._last_down_tick;
				}
				return false;
			}
			else if ((uType == WM_KEYUP) && last_tick)
			{
				bool go_event = false;
				//���ʱ��С��200ms,���ڼ�û�а��¹���������
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

	return is_handler;
}

LRESULT CALLBACK easyWDM::KeyHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0) return CallNextHookEx(_hookMouse, nCode, wParam, lParam);

	if (nCode == HC_ACTION && m_pThis->KeyMessage(wParam, (KBDLLHOOKSTRUCT*)lParam))
	{
		return true;
	}
	else {
		return CallNextHookEx(_hookKey, nCode, wParam, lParam);
	}
}