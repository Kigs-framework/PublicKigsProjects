#include "WebScraper.h"
#include "WebViewHandler.h"
#include "Timer.h"
#include <Windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include <thread>
#include <future>
#include <WebView2.h>

IMPLEMENT_CLASS_INFO(WebScraper);

HINSTANCE	WebScraper::mHinstance = NULL;


//! module init
void WebScraper::Init(KigsCore* core, const kstl::vector<CoreModifiableAttribute*>* params)
{
    ParentClassType::Init(core, params);
    DECLARE_CLASS_INFO_WITHOUT_FACTORY(WebScraper, "WebScraper");
    DECLARE_FULL_CLASS_INFO(core, WebViewHandler, WebViewHandler, WebScraper);
    BaseInit(core,"WebScraper",params);
}

//! module close
void WebScraper::Close()
{
    BaseClose();
	
}    

ModuleBase* ModuleInit(KigsCore* core, const kstl::vector<CoreModifiableAttribute*>* params)
{
	ModuleBase* newModule = new WebScraper("theWebScraper");
    newModule->Init(core, params);
	return newModule;
}

extern "C"
{

    __declspec(dllexport) BOOL WINAPI DllMain(
        HINSTANCE hinstDLL,  // handle to DLL module
        DWORD fdwReason,     // reason for calling function
        LPVOID lpReserved)  // reserved
    {

        WebScraper::SetHInstance(hinstDLL);

        // Perform actions based on the reason for calling.
        switch (fdwReason)
        {
        case DLL_PROCESS_ATTACH:
            // Initialize once for each new process.
            // Return FALSE to fail DLL load.
            break;

        case DLL_THREAD_ATTACH:
            // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
            // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
            break;
        }
        return TRUE;  // Successful DLL_PROCESS_ATTACH.
    }
}