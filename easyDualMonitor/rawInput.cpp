#include "easyWDM.h"
#include "helper.hpp"
#include "Resource.h"

constexpr auto rawInputClass = "easyRawInput";

LRESULT CALLBACK easyWDM::RawInputProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_INPUT:
			return true;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

void easyWDM::initRawInput()
{
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hInstance = ::GetModuleHandleA(nullptr);
	wcex.lpfnWndProc = RawInputProc;
	wcex.lpszClassName = rawInputClass;

	if (RegisterClassEx(&wcex) == 0)
	{
		throw std::runtime_error("RegisterClassEx Failed");
	}

	m_hRawInputWnd = ::CreateWindowA(rawInputClass, "XXX", 0, 0, 0, 0, 0, nullptr, nullptr, wcex.hInstance, nullptr);
	if (!m_hRawInputWnd)
	{
		throw std::runtime_error("CreateWindow Failed");
	}

	UpdateWindow(m_hRawInputWnd);

	RAWINPUTDEVICE rid;  //设备信息
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x06; //键盘   rid.usUsagePage = 0x01; rid.usUsage = 0x02; 为鼠标
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = m_hRawInputWnd;

	VERIFY(RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)));

	MSG msg;
	while (GetMessageA(&msg, nullptr, 0, 0))
	{
		//if (msg.message==WM_INPUT)
		{
			int x = 0;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
