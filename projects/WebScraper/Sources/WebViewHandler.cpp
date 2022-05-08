#include "WebViewHandler.h"
#include "WebScraper.h"

IMPLEMENT_CLASS_INFO(WebViewHandler)

using namespace Microsoft::WRL;


void	WebViewHandler::Create(HWND	hWnd)
{
	mHWnd = hWnd;
	CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(this, &WebViewHandler::creationCallback).Get());
}

void	WebViewHandler::Navigate(const std::string& url)
{
	mUrl = url;
	if (mWebviewWindow.get())
	{
		std::wstring lurl(url.begin(), url.end());
		mWebviewWindow->Navigate(lurl.c_str());
	}
}
HRESULT WebViewHandler::executeScriptCallback(HRESULT errorCode, LPCWSTR id)
{
	return S_OK;
}

void	WebViewHandler::ExecuteNext()
{
	mCanExecuteNext = false;
	std::wstring wscript = (wchar_t*)mScript.us_str();
	mScript = (std::string)"";
	mWebviewWindow->ExecuteScript(wscript.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(this, &WebViewHandler::executeScriptCallback).Get());
}

HRESULT	WebViewHandler::creationCallback(HRESULT result, ICoreWebView2Environment* env)
{
	if (result == S_OK)
	{
		env->CreateCoreWebView2Controller(mHWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(this, &WebViewHandler::controllerCreationCallback).Get());
	}
	return S_OK;
}

HRESULT WebViewHandler::controllerCreationCallback(HRESULT result, ICoreWebView2Controller* controller)
{
	if (controller != nullptr) {
		mWebviewController = controller;
		mWebviewController->get_CoreWebView2(&mWebviewWindow);
	}

	// Add a few settings for the webview
	// The demo step is redundant since the values are the default settings
	ICoreWebView2Settings* Settings;
	mWebviewWindow->get_Settings(&Settings);
	Settings->put_IsScriptEnabled(TRUE);
	Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
	Settings->put_IsWebMessageEnabled(TRUE);

	// Resize WebView to fit the bounds of the parent window
	RECT bounds;
	GetClientRect(mHWnd, &bounds);
	mWebviewController->put_Bounds(bounds);

	EventRegistrationToken token;
	mWebviewWindow->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(this, &WebViewHandler::receiveWebMessage).Get(), &token);

	EventRegistrationToken m_navigationCompletedToken;
	mWebviewWindow->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(this, &WebViewHandler::navigationCompleted).Get(), &m_navigationCompletedToken);

	if (((std::string)mUrl).size())
	{
		Navigate(mUrl);
	}
	return S_OK;
}

HRESULT WebViewHandler::receiveWebMessage(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args)
{
	
	bool	LaunchNext = false;
	PWSTR message;
	args->TryGetWebMessageAsString(&message);

	std::wstring	converted(message);
	std::string		s(converted.begin(), converted.end());

	EmitSignal(Signals::msgReceived, this,s);

	CoTaskMemFree(message);
	return S_OK;
}

HRESULT WebViewHandler::navigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
{
	wil::unique_cotaskmem_string uri;
	sender->get_Source(&uri);

	if (wcscmp(uri.get(), L"about:blank") == 0)
	{
		uri = wil::make_cotaskmem_string(L"");
	}
	
	std::wstring	converted(uri.get());
	std::string		s(converted.begin(), converted.end());

	mUrl = s;

	EmitSignal(Signals::navigationComplete, this);
	
	return S_OK;
}

void WebViewHandler::NotifyUpdate(const u32 labelid)
{
	if (labelid == mUrl.getID())
	{
		if ((std::string)mUrl != "")
		{
			Navigate(mUrl);
		}
	}
	else if (labelid == mScript.getID())
	{
		if (mScript.size())
		{
			auto a1 = std::async(&WebViewHandler::asyncExecute, this, (unsigned int)800, (wchar_t*)mScript.us_str());
		}

	}
	ParentClassType::NotifyUpdate(labelid);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	WebViewHandler* lWebView=(WebViewHandler*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_SIZE:
		
		if (lWebView)
		{
			if (lWebView->GetController() != nullptr)
			{
				RECT bounds;
				GetClientRect(hWnd, &bounds);
				lWebView->GetController()->put_Bounds(bounds);
			}
		}
		break;
	case WM_DESTROY:
		
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

void  WebViewHandler::InitModifiable()
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = WebScraper::GetHInstance();
	wcex.hIcon = LoadIcon(WebScraper::GetHInstance(), IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = _T("WebScraper");
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return;
	}

	// The parameters to CreateWindow explained:
	// szWindowClass: the name of the application
	// szTitle: the text that appears in the title bar
	// WS_OVERLAPPEDWINDOW: the type of window to create
	// CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
	// 500, 100: initial size (width, length)
	// NULL: the parent of this window
	// NULL: this application does not have a menu bar
	// hInstance: the first parameter from WinMain
	// NULL: not used in this application
	HWND hWnd = CreateWindow(
		_T("WebScraper"),
		_T("WebScraper Window") ,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1200, 900,
		NULL,
		NULL,
		wcex.hInstance,
		NULL
	);

	if (!hWnd)
	{
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return;
	}

	SetWindowLongPtr(hWnd, GWLP_USERDATA,(LONG_PTR)this);


	// The parameters to ShowWindow explained:
	// hWnd: the value returned from CreateWindow
	// nCmdShow: the fourth parameter from WinMain
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	Create(hWnd);

//	Navigate(L"https://twitter.com/elegantthemes/status/1107776313505374208/likes");

}

void WebViewHandler::Update(const Timer& timer, void* addParam)
{

	// Main message loop:
	MSG msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (CanExecuteNext())
	{
		ExecuteNext();
	}
}
