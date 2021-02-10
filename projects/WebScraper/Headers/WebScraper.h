#ifndef _WEBSCRAPER_H_
#define _WEBSCRAPER_H_

#include "AnonymousModule.h"
#include "AttributePacking.h"
#include <Windows.h>

// ****************************************
// * WebScraper class
// * --------------------------------------
/**
 * \file	WebScraper.h
 * \class	WebScraper
 * \ingroup Module
 * \brief	anonymous module Win32 WebScraper based on WebView2.
 */
 // ****************************************

class WebScraper : public BaseDllModule
{
public:
	DECLARE_CLASS_INFO(WebScraper, BaseDllModule, testDll)
	DECLARE_INLINE_CONSTRUCTOR(WebScraper)
	{

	}
    
	//! module init
    void Init(KigsCore* core, const kstl::vector<CoreModifiableAttribute*>* params) override;
	
	//! module close
    void Close() override;

	void Update(const Timer& timer, void* addParam) override
	{
	}

	static HINSTANCE GetHInstance()
	{
		return mHinstance;
	}

	static void SetHInstance(HINSTANCE toSet)
	{
		mHinstance = toSet;
	}
	
protected:

	static HINSTANCE	mHinstance;
}; 

extern "C"
{
    __declspec(dllexport) ModuleBase* ModuleInit(KigsCore* core, const kstl::vector<CoreModifiableAttribute*>* params);
}
#endif //_WEBSCRAPER_H_
