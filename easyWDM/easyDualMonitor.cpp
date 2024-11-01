// trayicondemo.cpp : 定义应用程序的入口点。
#include "pch.h"
#include "framework.h"
#include "easyDualMonitor.h"

using namespace easy;

#include "easyWDM.h"

#define MID_DESKTOP 1
#define MID_RUN 2
#define MID_MOVE_WND 3
#define MID_LIMIT 4
#define MID_AUTO_RUN 5
#define MID_RELOAD_CONFIG 6

static constexpr auto app_name = "easyWDM";

bool enumElement(IUIAutomation* pAutomation
	, DWORD dwProcessId
	, LPCSTR szAutomationId
	, IUIAutomationElement* pElement
	, std::function<bool(IUIAutomationElement* pElement)>&& cb)
{
	/*

	auto pCond = create_condition(pAutomation, UIA_AutomationIdPropertyId, _variant_t(etd::string::ansi_unicode(szAutomationId).c_str()), nullptr);

	IUIAutomationElementArray* pChildren = nullptr;
	console.time();
	auto hr = pElement->FindAll(TreeScope_Subtree, pCond, &pChildren);
	console.timeEnd();
	if (!pChildren) return false;

	int count = 0;
	pChildren->get_Length(&count);

	for (int i = 0; i < count; ++i) {
		IUIAutomationElement* pChild = nullptr;
		hr = pChildren->GetElement(i, &pChild);
		if (SUCCEEDED(hr) && pChild)
		{
			// 处理每个元素
			pChild->Release();
		}
	}

	*/
	return false;
}


int APIENTRY WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);


/*
	auto hTaskBar = helper::getCurrentMonitorTaskBarWnd();
	if (hTaskBar)
	{
		easy_uiautomation uiAuto(hTaskBar);
		if (uiAuto)
		{
			uiAuto.AddCond(UIA_AutomationIdPropertyId, etd::string::ansi_unicode("StartButton").c_str());
			auto butStart = uiAuto.FindElement();

			do 
			{
				butStart.ToggleState();
				::Sleep(1000);
			} while (true);
		}
	}
	return 0;*/


	helper::m_runDlgIcon = LoadIcon(::GetModuleHandleA(nullptr), MAKEINTRESOURCEA(IDI_TRAYICONDEMO));

	//FilePath fp("C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise");
	//auto aa= fp.is_exists();

	//设置exe所在目录为当前目录
	process.set_current_dir("");

	worker.startWork(5);

	console.set_logfile();
	console.log("启动");

	process.set_app_name(app_name);

	if (process.is_already_run())
	{
		process.exit("不允许重复运行!");
		return 0;
	}

	tray_icon tray(IDI_TRAYICONDEMO, app_name);

	easyWDM wdm(tray);

	tray.AddMenu("开机自动运行", MID_AUTO_RUN, [&]()
	{
		bool is_enable = !tray.GetCheck();
		if (process.set_autorun(is_enable))
		{
			tray.SetCheck(is_enable);
		}
		//tray.show_info("test");
	});

	tray.AddSeparator();

	/*
		tray.AddMenu("重新运行(&R)", [&]()
			{
				tray.DeleteTray();

			});*/

			//tray.AddSeparator();
	tray.AddMenu("退出(&X)", [&]()
	{
		tray.close();
	});

	tray.SetCheck(MID_AUTO_RUN, process.is_autorun());

	wdm.initWDM();

	tray.run();

	worker.stop();

	CoUninitialize();

	return 0;
}