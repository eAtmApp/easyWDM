#pragma once
#include <windows.h>

#include "helper.hpp"

using namespace easy;

typedef std::function<bool()> hotkey_handler;

class easyWDM
{
public:
	easyWDM(tray_icon& tray);
	~easyWDM();

	//清除钩子
	void	UninstallHook();

	//安装钩子
	bool	initWDM();

	//读取配置文件
	bool	initConfig();

	bool	SetHotkey(std::string hotkey, hotkey_handler&& handler);

	bool	init_hid();

private:
	
	//坐标得到显示器句柄
	HMONITOR	getMonitor(POINT &pt);

	//输出显示器信息
	void	refreshMonitor();

	HWND	m_hRawInputWnd = nullptr;

	MONITOR_INFOS	m_monitors;

	//开启 或 关闭 限制鼠标
	bool	set_limit_mouse();

	bool	m_bIs_limit_mouse = true;

	bool	is_match(eString configName, eStringV psName, eStringV title, eStringV cls);

	void	WndHookProc(HWND hWnd, bool isCreate);
	std::map<HWND, DWORD> _mapWndTick;

	bool	KeyMessage(UINT uType, KBDLLHOOKSTRUCT* pHook);

	bool	MouseMessage(UINT button, POINT pt);

	bool	m_filter_win_status = false;

#define VK_WIN 0xFF

	enum key_flags
	{
		FLAGS_CTRL = 0x1,
		FLAGS_ALT = 0x2,
		FLAGS_SHIFT = 0x4,
		FLAGS_WIN = 0x8,
	};

	struct _KEYS_STATUS
	{
		UCHAR keys[0xFF] = { 0 };

		//如果是down,将会返回此次down重复消息次数
		UCHAR inline set_status(UINT uType, UCHAR cKey)
		{
			UCHAR uDownCount = 0;
			ASSERT(cKey <= 0xFF);
			if (cKey >= 0xFF) return 0;
			if (uType == WM_KEYDOWN)
			{
				uDownCount=++keys[cKey];
			}
			else {
				keys[cKey] = 0;
			}
			//keys[cKey] = uType == WM_KEYDOWN;

			//记录第一次按下的时间,按住时也会引发此消息,则不设置
			//if (uType == WM_KEYDOWN && uDownCount==1) _last_down_tick = ::GetTickCount64();
			if (uType == WM_KEYDOWN ) _last_down_tick = ::GetTickCount64();

			return uDownCount;
		}
		
		bool inline is_down(UCHAR cKey)
		{
			switch (cKey)
			{
				case VK_CONTROL:
					return keys[VK_LCONTROL] || keys[VK_RCONTROL];
				case VK_MENU:
					return keys[VK_LMENU] || keys[VK_RMENU];
					break;
				case VK_SHIFT:
					return keys[VK_LSHIFT] || keys[VK_RSHIFT];
					break;
				case VK_WIN:
					return keys[VK_LWIN] || keys[VK_RWIN];
					break;
			}
			return keys[cKey];
		}

		//得到当前的hotkey flags,不分左右
		DWORD	inline get_hotkey_flags()
		{
			WORD flags = 0;
			if (keys[VK_LCONTROL] || keys[VK_RCONTROL]) flags |= FLAGS_CTRL;
			if (keys[VK_LMENU] || keys[VK_RMENU]) flags |= FLAGS_ALT;
			if (keys[VK_LSHIFT] || keys[VK_RSHIFT]) flags |= FLAGS_SHIFT;
			if (keys[VK_LWIN] || keys[VK_RWIN]) flags |= FLAGS_WIN;
			return flags;
		}
		//不分左右
		DWORD	inline get_hotkey_flags_noBy(WORD noKey)
		{
			return get_hotkey_flags() ^ noKey;
		}

		inline static DWORD keyString_to_Flags(etd::string hotkey_str)
		{
			hotkey_str.toupper();
			auto vecKey = hotkey_str.split("+");
			DWORD flags = 0;
			DWORD key = 0;

			for (auto& keystr : vecKey)
			{
				if (keystr == "CTRL") flags |= FLAGS_CTRL;
				else if (keystr == "ALT") flags |= FLAGS_ALT;
				else if (keystr == "SHIFT") flags |= FLAGS_SHIFT;
				else if (keystr == "WIN") flags |= FLAGS_WIN;
				else if (key == 0 && keystr.size() >= 1)
				{
				#define IS_NUMBER(x) (x>='0' && x<='9')
				#define IS_LEETR(x) (x>='A' && x<='Z')

					if (keystr.size() == 1 && (IS_NUMBER(keystr[0]) || IS_LEETR(keystr[0])))
					{
						//0~9,A~Z
						key = keystr[0];
					}
					else if (keystr[0] == 'F' && (keystr.size() == 2 || keystr.size() == 3))
					{
						etd::string estr(keystr.data() + 1, keystr.size() - 1);
						auto val = estr.to_int32();
						if (val >= 1 && val <= 24)
						{
							key = val + VK_F1 - 1;
						}
						else {
							return 0;
						}
					}
					else {
						return 0;
					}
				}
				else {
					return 0;
				}
			}

			flags = flags << 16;
			flags |= key;
			return flags;
		}
		inline DWORD get_cur_flags(UCHAR cKey)
		{
			DWORD flags = get_hotkey_flags();
			flags = flags << 16;
			flags |= cKey;
			return flags;
		}
		inline static DWORD marge_flags(DWORD hotkeyFlags, UCHAR cKey)
		{
			hotkeyFlags = hotkeyFlags << 16;
			hotkeyFlags |= cKey;
			return hotkeyFlags;
		}

		void	reset()
		{
			_last_down_tick = 0;
			RtlZeroMemory(keys, sizeof(keys));
		}

		//记录最后一次按下按键时间
		uint64_t _last_down_tick = 0;
	};

	//处理Win+X键热键消息-如果处理过滤返回true
	bool	HandlerHotkey(UCHAR ukey, DWORD hotkey_flags);

	std::unordered_map<DWORD, hotkey_handler> _map_hotkey;

	void	bluetooth_check();
private:

	inline static _KEYS_STATUS key_status;

	static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK KeyHookProc(int nCode, WPARAM wParam, LPARAM lParam);
	inline static	UINT	g_Shell_Wnd_Msg_ID = 0;
	inline static HHOOK	_hookMouse = nullptr;
	inline static HHOOK	_hookKey = nullptr;

	inline static easyWDM* m_pThis = nullptr;
	tray_icon& _tray;

	jsoncpp	_config;

	eString _BluetoothMac;
};