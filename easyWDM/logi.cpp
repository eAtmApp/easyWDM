#include "pch.h"
#include "easyWDM.h"
#include<Hidsdi.h>
#include <Setupapi.h>
#pragma comment(lib,"Setupapi.lib")
#pragma comment(lib,"hid.lib")

#include <devguid.h>

bool easyWDM::init_hid()
{
	int x = 0;

	{
		auto hidName = R"(\\?\hid#vid_046d&pid_c52b&mi_02&col01#b&d0d5a7e&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030})";

		DWORD dwAccess = GENERIC_READ | GENERIC_WRITE;
		HANDLE hFile = CreateFileA(hidName, dwAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

		eStringArray arr;
		arr.push_back("10FF8100000000");
		arr.push_back("10FF8000000900");
		arr.push_back("10FF8102000000");
		arr.push_back("10FF8002020000");
		arr.push_back("10FF001A000000");
		arr.push_back("10FF81F1010000");
		arr.push_back("10FF81F1020000");
		arr.push_back("1001001A000091");
		arr.push_back("1001050A000000");
		arr.push_back("10FF8100000000");
		arr.push_back("10FF8000000900");
		arr.push_back("1001040A000000");
		arr.push_back("1001041A000000");
		arr.push_back("1001041A014A00");
		arr.push_back("1001142A000303");

		for (auto& hex : arr)
		{
			auto bin = util::hex_to_binary(hex);
			DWORD dwSize = 0;
			if (!::WriteFile(hFile, bin.data(), bin.size(), &dwSize, nullptr))
			{
				console.log("–¥»Î ß∞‹:{}", hex);
			}
		}

		auto hid2 = R"(\\?\hid#vid_1c4f&pid_0082&mi_01&col06#9&6dccce6&0&0005#{4d1e55b2-f16f-11cf-88cb-001111000030}})";
		HANDLE hFile2 = CreateFileA(hidName, dwAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

		auto bin = util::hex_to_binary("11010D3A00520300520000000000000000000000");
		DWORD dwSize = 0;
		auto rrr = ::WriteFile(hFile, bin.data(), bin.size(), &dwSize, nullptr);
		rrr = rrr;

		/*
				do
				{
					char szBuffer[64] = { 0 };
					DWORD dwReadSize = 0;
					if (!::ReadFile(hFile, szBuffer, sizeof(szBuffer), &dwReadSize, nullptr))
					{
						box.ApiError("ReadFile");
					}
					auto readstr = util::binary_to_hex({ szBuffer, dwReadSize });
					console.log("∂¡µΩ:{}", readstr);
				} while (true);*/

				//return false;


	}

	GUID guid;
	HidD_GetHidGuid(&guid);

	auto hDev = SetupDiGetClassDevs(&guid, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);// DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (hDev == INVALID_HANDLE_VALUE) return false;

	// enumerute device information
	DWORD required_size = 0;

	SP_DEVINFO_DATA DeviceInfoData = { sizeof(DeviceInfoData) };

	SP_DEVICE_INTERFACE_DATA ifdata = { 0 };
	ifdata.cbSize = sizeof(ifdata);

	GUID mouse_guid = GUID_DEVCLASS_MOUSE;

	//HidP_GetButtonCaps
	//HidP_GetSpecificButtonCaps

	for (DWORD devindex = 0; SetupDiEnumDeviceInterfaces(hDev, nullptr, &guid, devindex, &ifdata); ++devindex)
	{
		HANDLE hFile = INVALID_HANDLE_VALUE;
		PHIDP_PREPARSED_DATA pp_data = nullptr;
		do
		{
			char szPathBuffer[2048] = { 0 };
			DWORD dwBufferSize = sizeof(szPathBuffer);
			auto& devdetail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A&)szPathBuffer;
			devdetail.cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

			SP_DEVINFO_DATA devinfo = { sizeof(SP_DEVINFO_DATA) };
			if (!SetupDiGetDeviceInterfaceDetailA(hDev, &ifdata, &devdetail, dwBufferSize, nullptr, &devinfo)) continue;

			hFile = CreateFileA(devdetail.DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			if (hFile == INVALID_HANDLE_VALUE)  break;;

			HIDD_ATTRIBUTES attr = { 0 };
			attr.Size = sizeof(HIDD_ATTRIBUTES);
			if (!::HidD_GetAttributes(hFile, &attr)) break;

			if (!HidD_GetPreparsedData(hFile, &pp_data)) break;

			HIDP_CAPS caps = { 0 };
			auto nt_res = HidP_GetCaps(pp_data, &caps);
			if (nt_res != HIDP_STATUS_SUCCESS) break;

			if (caps.UsagePage != 0xFF00) break;

			//HidP_GetButtonCaps()

			HIDP_BUTTON_CAPS butcps[16] = { 0 };
			USHORT ul = 16;
			auto gbc=HidP_GetButtonCaps(HidP_Input, butcps, &ul,pp_data);

			console.log("{}", devdetail.DevicePath);
			console.log("VendorID:{} ProductID:{} VersionNumber:{}\n", attr.VendorID, attr.ProductID, attr.VersionNumber);
			console.log("UsagePage:{:4X}, Usage:{:4X}\n", caps.UsagePage, caps.Usage);
			char szName[2048] = { 0 };
			if (HidD_GetProductString(hFile, szName, sizeof(szName)))
			{
				wchar_t* cao = (wchar_t*)szName;
				auto astr = util::unicode_ansi(cao);
				console.log("{}\n", astr);
			}

			console.log("\r\n");
		} while (false);

		if (pp_data)HidD_FreePreparsedData(pp_data);
		if (hFile != INVALID_HANDLE_VALUE) ::CloseHandle(hFile);
	}

	SetupDiDestroyDeviceInfoList(hDev);

	return false;
}