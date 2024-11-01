#pragma once
#include <UIAutomation.h>
#include <comutil.h>
#pragma comment(lib, "Ole32.lib")  // 确保链接到 Ole32.lib
#pragma comment(lib, "comsuppw.lib")  // 确保链接到 comsuppw.lib
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
		_hWnd = src._hWnd;
		_dwProcessId = src._dwProcessId;
	}

	easy_uiautomation()
	{
	}
	easy_uiautomation(HWND hWnd)
	{
		FromHandle(hWnd);
	}

	easy_uiautomation(CComPtr<IUIAutomation> pAutomation, CComPtr<IUIAutomationElement> pElement, DWORD dwProcessId, HWND hWnd)
	{
		Clear();
		_dwProcessId = dwProcessId;
		_hWnd = hWnd;
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

	//从HWND句柄得到对象
	bool FromHandle(HWND hWnd)
	{
		if (!_pAutomation)
		{
			//auto hr=CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&_pAutomation);
			auto hr = _pAutomation.CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER);
		}

		auto hr = _pAutomation->ElementFromHandle(hWnd, &_pElement);

		if (_pElement)
		{
			_hWnd = hWnd;
			::GetWindowThreadProcessId(hWnd, &_dwProcessId);
		}

		return _pElement != nullptr;
	}

	//检测对象窗口是否正常
	bool	CheckObject()
	{
		if (!::IsWindow(_hWnd)) return false;

		DWORD dwProcessId = 0;
		::GetWindowThreadProcessId(_hWnd, &dwProcessId);
		if (dwProcessId != _dwProcessId)
		{
			return false;
		}
		return true;
	}

	//按照条件查找元素
	easy_uiautomation FindElement()
	{
		CComPtr<IUIAutomationElement> pElement = nullptr;
		auto hr = _pElement->FindFirst(TreeScope_Subtree, _pCond, &pElement);

		easy_uiautomation ele(_pAutomation, pElement, _dwProcessId, _hWnd);

		return ele;
	}

	bool	ToggleState()
	{
		VARIANT toggleState;
		auto hr = _pElement->GetCurrentPropertyValue(UIA_ToggleToggleStatePropertyId, &toggleState);
		return toggleState.intVal == ToggleState_On;
	}

	//点击元素
	bool	ClickElement()
	{
		// 执行点击操作
		CComPtr< IUIAutomationInvokePattern> pInvokePattern = nullptr;

		auto hr = _pElement->GetCurrentPattern(UIA_InvokePatternId, (IUnknown**)&pInvokePattern);
		if (!pInvokePattern)
		{
			hr = _pElement->GetCurrentPattern(UIA_TogglePatternId, (IUnknown**)&pInvokePattern);
		}
		
		if (pInvokePattern)
		{
			hr = pInvokePattern->Invoke();
			if (hr != S_OK)
			{
				console.error("UIA_InvokePatternId->Invoke失败");
			}
			return true;
		}
		else {
			console.error("没有UIA_InvokePatternId接口");
		}
		return false;
	}

	//清空条件
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

	//创建/合并条件
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

	HWND	_hWnd = nullptr;
	DWORD	_dwProcessId = 0;
};