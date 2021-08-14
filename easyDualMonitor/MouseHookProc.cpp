#include "easyWDM.h"

LRESULT CALLBACK easyWDM::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0) return CallNextHookEx(_hookMouse, nCode, wParam, lParam);

/*
	if (nCode == HC_ACTION && m_pThis->MouseMessage(wParam, (KBDLLHOOKSTRUCT*)lParam))
	{
		return true;
	}
	else {
		return CallNextHookEx(_hookKey, nCode, wParam, lParam);
	}*/
	return CallNextHookEx(_hookMouse, nCode, wParam, lParam);
}