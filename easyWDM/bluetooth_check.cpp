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
	BLUETOOTH_FIND_RADIO_PARAMS btfrp = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) }; //����BluetoothFindFirstDevice�������������շ�������Ҫ�������������� 
	BLUETOOTH_RADIO_INFO bri = { sizeof(BLUETOOTH_RADIO_INFO) }; //��ʼ��һ�����������շ�����Ϣ��BLUETOOTH_RADIO_INFO���Ķ���bri
	BLUETOOTH_DEVICE_SEARCH_PARAMS btsp = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };//����BluetoothFindFirstDevice����������Ҫ�������������� 
	BLUETOOTH_DEVICE_INFO btdi = { sizeof(BLUETOOTH_DEVICE_INFO) };  //��ʼ��һ��Զ�������豸��Ϣ��BLUETOOTH_DEVICE_INFO������btdi���Դ����������������豸��Ϣ
	hbf = BluetoothFindFirstRadio(&btfrp, &hbr); //�õ���һ����ö�ٵ������շ����ľ��hbf������BluetoothFindNextRadio��hbr������BluetoothFindFirstDevice����û���ҵ������������շ�������õ��ľ��hbf=NULL������ɲο�https://msdn.microsoft.com/en-us/library/aa362786(v=vs.85).aspx 

	bool brfind = hbf != NULL;
	while (brfind)
	{
		if (BluetoothGetRadioInfo(hbr, &bri) == ERROR_SUCCESS)//��ȡ�����շ�������Ϣ��������bri��  
		{
			cout << "Class of device: 0x" << uppercase << hex << bri.ulClassofDevice << endl;
			wcout << "Name:" << bri.szName << endl;  //�����շ���������
			cout << "Manufacture:0x" << uppercase << hex << bri.manufacturer << endl;
			cout << "Subversion:0x" << uppercase << hex << bri.lmpSubversion << endl;
			//  
			btsp.hRadio = hbr;  //����ִ�������豸���ڵľ����Ӧ��Ϊִ��BluetoothFindFirstRadio�������õ��ľ��
			btsp.fReturnAuthenticated = TRUE;//�Ƿ���������Ե��豸  
			btsp.fReturnConnected = FALSE;//�Ƿ����������ӵ��豸  
			btsp.fReturnRemembered = TRUE;//�Ƿ������Ѽ�����豸  
			btsp.fReturnUnknown = TRUE;//�Ƿ�����δ֪�豸  
			btsp.fIssueInquiry = TRUE;//�Ƿ�����������True��ʱ���ִ���µ�������ʱ��ϳ���FALSE��ʱ���ֱ�ӷ����ϴε����������
			btsp.cTimeoutMultiplier = 30;//ָʾ��ѯ��ʱ��ֵ����1.28��Ϊ������ ���磬12.8��Ĳ�ѯ��cTimeoutMultiplierֵΪ10.�˳�Ա�����ֵΪ48.��ʹ�ô���48��ֵʱ�����ú�������ʧ�ܲ����� 
			hbdf = BluetoothFindFirstDevice(&btsp, &btdi);//ͨ���ҵ���һ���豸�õ���HBLUETOOTH_DEVICE_FIND���hbdf��ö��Զ�������豸���ѵ��ĵ�һ��Զ�������豸����Ϣ������btdi�����С���û��Զ�������豸��hdbf=NULL��  
			bool bfind = hbdf != NULL;
			while (bfind)
			{
				console.log(util::unicode_ansi(btdi.szName));
				wcout << "[Name]:" << btdi.szName;  //Զ�������豸������
				cout << ",[Address]:0x" << uppercase << hex << btdi.Address.ullLong << endl;
				bfind = BluetoothFindNextDevice(hbdf, &btdi);//ͨ��BluetoothFindFirstDevice�õ���HBLUETOOTH_DEVICE_FIND�����ö��������һ��Զ�������豸������Զ�������豸����Ϣ������btdi��  
			}
			BluetoothFindDeviceClose(hbdf);//ʹ�����ǵùر�HBLUETOOTH_DEVICE_FIND���hbdf��  
		}
		CloseHandle(hbr);
		brfind = BluetoothFindNextRadio(hbf, &hbr);//ͨ��BluetoothFindFirstRadio�õ���HBLUETOOTH_RADIO_FIND���hbf��ö��������һ�����������շ������õ�������BluetoothFindFirstDevice�ľ��hbr��    
	}
	BluetoothFindRadioClose(hbf);//ʹ�����ǵùر�HBLUETOOTH_RADIO_FIND���hbf��

}