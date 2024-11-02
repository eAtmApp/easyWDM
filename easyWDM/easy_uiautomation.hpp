#pragma once
#include <UIAutomation.h>
#include <comutil.h>
#pragma comment(lib, "Ole32.lib")  // ȷ�����ӵ� Ole32.lib
#pragma comment(lib, "comsuppw.lib")  // ȷ�����ӵ� comsuppw.lib
#include <atlbase.h>

using namespace easy;

class easy_uiautomation
{
public:

	easy_uiautomation(easy_uiautomation& src)
	{
		Copy(src);
	}

	void Copy(easy_uiautomation& src)
	{
		_pAutomation = src._pAutomation;
		_pElement = src._pElement;
	}

	easy_uiautomation()
	{
	}
	easy_uiautomation(HWND hWnd)
	{
		FromHandle(hWnd);
	}
	 
	easy_uiautomation(CComPtr<IUIAutomation> pAutomation, CComPtr<IUIAutomationElement> pElement)
	{
		Clear();
		_pAutomation = pAutomation;
		_pElement = pElement;
	}

	void Clear()
	{
		_pElement = nullptr;
		_pAutomation = nullptr;
		_pCond = nullptr;
	}

	~easy_uiautomation()
	{
		Clear();
	}

	//��HWND����õ�����
	bool FromHandle(HWND hWnd)
	{
		if (!_pAutomation)
		{
			//auto hr=CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&_pAutomation);
			auto hr = _pAutomation.CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER);
		}

		auto hr = _pAutomation->ElementFromHandle(hWnd, &_pElement);
		
		return _pElement != nullptr;
	}

	//������������Ԫ��
	easy_uiautomation FindElement()
	{
		CComPtr<IUIAutomationElement> pElement = nullptr;
		auto hr = _pElement->FindFirst(TreeScope_Subtree, _pCond, &pElement);

		easy_uiautomation ele(_pAutomation, pElement);

		return ele;
	}

	bool	ToggleState()
	{
		VARIANT toggleState;
		auto hr = _pElement->GetCurrentPropertyValue(UIA_ToggleToggleStatePropertyId, &toggleState);
		return toggleState.intVal == ToggleState_On;
	}

	//���Ԫ��
	bool	ClickElement()
	{
		// ִ�е������
		CComPtr< IUIAutomationInvokePattern> pPattern = nullptr;
		
		
		auto hr = _pElement->GetCurrentPattern(UIA_InvokePatternId, (IUnknown**)&pPattern);
		if (pPattern)
		{
			hr = pPattern->Invoke();
			if (hr != S_OK)
			{
				console.error("UIA_InvokePatternId->Invokeʧ��");
				return false;
			}
			return true;
		}
		else {
			CComPtr< IUIAutomationTogglePattern> pToggle = nullptr;
			hr = _pElement->GetCurrentPattern(UIA_TogglePatternId, (IUnknown**)&pToggle);

			if (pToggle)
			{
				hr = pToggle->Toggle();
				if (hr != S_OK)
				{
					console.error("UIA_TogglePatternId->Toggleʧ��");
					return false;
				}
				return true;
			}
			else {
				console.error("û��UIA_InvokePatternId��UIA_TogglePatternId�ӿ�");
				return false;
			}
			
			return false;
		}
		

	}

	//�������
	void	ClearCond()
	{
		if (_pCond)
		{
			_pCond = nullptr;
		}
	}

	operator bool() const {
		return _pElement != nullptr;
	}

	//����/�ϲ�����
	template <typename T>
	bool AddCond(PROPERTYID iid, T&& value)
	{
		_variant_t val(std::move(value));

		CComPtr< IUIAutomationCondition> pNewCond = nullptr;
		auto hr = _pAutomation->CreatePropertyCondition(iid, val, &pNewCond);
		if (!pNewCond) return false;

		if (!_pCond)
		{
			_pCond = pNewCond;
			return true;
		}

		CComPtr< IUIAutomationCondition> pTemp;
		_pAutomation->CreateAndCondition(_pCond, pNewCond, &pTemp);

		pNewCond = nullptr;

		if (!pTemp) return false;

		_pCond = pTemp;
		return true;
	}

private:
	//IUIAutomation* _pAutomation = nullptr;
	CComPtr<IUIAutomationElement> _pElement = nullptr;
	CComPtr<IUIAutomationCondition> _pCond = nullptr;
	CComPtr<IUIAutomation> _pAutomation = nullptr;
};