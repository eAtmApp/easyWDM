#include "pch.h"
#include "easyWDM.h"

#include <windows.h>
#include <BluetoothAPIs.h>
#include <conio.h>
#include <iostream>
#include <string>
#include <locale>

#pragma comment(lib,"Bthprops.lib")

using namespace std;

void easyWDM::bluetooth_check()
{

	HBLUETOOTH_RADIO_FIND hbf = NULL;
	HANDLE hbr = NULL;
	HBLUETOOTH_DEVICE_FIND hbdf = NULL;
	BLUETOOTH_FIND_RADIO_PARAMS btfrp = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) }; //调用BluetoothFindFirstDevice搜索本机蓝牙收发器所需要的搜索参数对象 
	BLUETOOTH_RADIO_INFO bri = { sizeof(BLUETOOTH_RADIO_INFO) }; //初始化一个储存蓝牙收发器信息（BLUETOOTH_RADIO_INFO）的对象bri
	BLUETOOTH_DEVICE_SEARCH_PARAMS btsp = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };//调用BluetoothFindFirstDevice搜索本所需要的搜索参数对象 
	BLUETOOTH_DEVICE_INFO btdi = { sizeof(BLUETOOTH_DEVICE_INFO) };  //初始化一个远程蓝牙设备信息（BLUETOOTH_DEVICE_INFO）对象btdi，以储存搜索到的蓝牙设备信息
	hbf = BluetoothFindFirstRadio(&btfrp, &hbr); //得到第一个被枚举的蓝牙收发器的句柄hbf可用于BluetoothFindNextRadio，hbr可用于BluetoothFindFirstDevice。若没有找到本机的蓝牙收发器，则得到的句柄hbf=NULL，具体可参考https://msdn.microsoft.com/en-us/library/aa362786(v=vs.85).aspx 

	bool brfind = hbf != NULL;
	while (brfind)
	{
		if (BluetoothGetRadioInfo(hbr, &bri) == ERROR_SUCCESS)//获取蓝牙收发器的信息，储存在bri中  
		{
			cout << "Class of device: 0x" << uppercase << hex << bri.ulClassofDevice << endl;
			wcout << "Name:" << bri.szName << endl;  //蓝牙收发器的名字
			cout << "Manufacture:0x" << uppercase << hex << bri.manufacturer << endl;
			cout << "Subversion:0x" << uppercase << hex << bri.lmpSubversion << endl;
			//  
			btsp.hRadio = hbr;  //设置执行搜索设备所在的句柄，应设为执行BluetoothFindFirstRadio函数所得到的句柄
			btsp.fReturnAuthenticated = TRUE;//是否搜索已配对的设备  
			btsp.fReturnConnected = FALSE;//是否搜索已连接的设备  
			btsp.fReturnRemembered = TRUE;//是否搜索已记忆的设备  
			btsp.fReturnUnknown = TRUE;//是否搜索未知设备  
			btsp.fIssueInquiry = TRUE;//是否重新搜索，True的时候会执行新的搜索，时间较长，FALSE的时候会直接返回上次的搜索结果。
			btsp.cTimeoutMultiplier = 30;//指示查询超时的值，以1.28秒为增量。 例如，12.8秒的查询的cTimeoutMultiplier值为10.此成员的最大值为48.当使用大于48的值时，调用函数立即失败并返回 
			hbdf = BluetoothFindFirstDevice(&btsp, &btdi);//通过找到第一个设备得到的HBLUETOOTH_DEVICE_FIND句柄hbdf来枚举远程蓝牙设备，搜到的第一个远程蓝牙设备的信息储存在btdi对象中。若没有远程蓝牙设备，hdbf=NULL。  
			bool bfind = hbdf != NULL;
			while (bfind)
			{
				console.log(util::unicode_ansi(btdi.szName));
				wcout << "[Name]:" << btdi.szName;  //远程蓝牙设备的名字
				cout << ",[Address]:0x" << uppercase << hex << btdi.Address.ullLong << endl;
				bfind = BluetoothFindNextDevice(hbdf, &btdi);//通过BluetoothFindFirstDevice得到的HBLUETOOTH_DEVICE_FIND句柄来枚举搜索下一个远程蓝牙设备，并将远程蓝牙设备的信息储存在btdi中  
			}
			BluetoothFindDeviceClose(hbdf);//使用完后记得关闭HBLUETOOTH_DEVICE_FIND句柄hbdf。  
		}
		CloseHandle(hbr);
		brfind = BluetoothFindNextRadio(hbf, &hbr);//通过BluetoothFindFirstRadio得到的HBLUETOOTH_RADIO_FIND句柄hbf来枚举搜索下一个本地蓝牙收发器，得到可用于BluetoothFindFirstDevice的句柄hbr。    
	}
	BluetoothFindRadioClose(hbf);//使用完后记得关闭HBLUETOOTH_RADIO_FIND句柄hbf。

}