#pragma once
#include "CoreModifiable.h"
#include "maString.h"
#include <Windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include <thread>
#include <future>
#include <WebView2.h>

// ****************************************
// * WebViewHandler class
// * --------------------------------------
/**
 * \file	WebViewHandler.h
 * \class	WebViewHandler
 * \brief	the web view handler class you can control by changing mUrl and mScript 
 *			and connect to signals msgReceived and navigationComplete
 *       
 */
 // ****************************************


class WebViewHandler : public CoreModifiable
{
public:
	DECLARE_CLASS_INFO(WebViewHandler, CoreModifiable, WebScraper);
	DECLARE_INLINE_CONSTRUCTOR(WebViewHandler)
	{
		mUrl.changeNotificationLevel(Owner);
		mScript.changeNotificationLevel(Owner);
	}

	void	Create(HWND	hWnd);
	void	Navigate(const std::string& url);

	wil::com_ptr<ICoreWebView2Controller> GetController()
	{
		return mWebviewController;
	}

	bool CanExecuteNext()
	{
		return mCanExecuteNext;
	}

	void	ExecuteNext();

	friend LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

protected:
	void InitModifiable() override;
	void NotifyUpdate(const u32 labelid) override;

	void Update(const Timer& timer, void* addParam) override;

	SIGNALS(msgReceived,navigationComplete);

	bool	mCanExecuteNext = false;

	void	asyncExecute(unsigned int ms, const std::wstring script)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		mCanExecuteNext = true;
	}

	// Pointer to WebViewController
	wil::com_ptr<ICoreWebView2Controller>	mWebviewController = nullptr;

	// Pointer to WebView window
	wil::com_ptr<ICoreWebView2>				mWebviewWindow = nullptr;

	HWND									mHWnd;

	HRESULT	creationCallback(HRESULT result, ICoreWebView2Environment* env);

	HRESULT controllerCreationCallback(HRESULT result, ICoreWebView2Controller* controller);

	HRESULT receiveWebMessage(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args);

	HRESULT navigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args);

	HRESULT executeScriptCallback(HRESULT errorCode, LPCWSTR id);

	maString		mUrl = BASE_ATTRIBUTE(Url,"");
	maUSString		mScript = BASE_ATTRIBUTE(Script, "");
};

