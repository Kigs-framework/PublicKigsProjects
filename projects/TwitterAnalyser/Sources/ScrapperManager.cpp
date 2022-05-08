#include "ScrapperManager.h"
#include "FilePathManager.h"
#include "ModuleFileManager.h"
#include "TwitterConnect.h"

IMPLEMENT_CLASS_INFO(ScrapperManager)

ScrapperManager::ScrapperManager(const kstl::string& name, CLASS_NAME_TREE_ARG) : CoreModifiable(name, PASS_CLASS_NAME_TREE_ARG)
{
	
}

void ScrapperManager::InitModifiable()
{
	if (IsInit())
		return;

	CoreModifiable::InitModifiable();

	// load anonymous module for web scraper
#ifdef _DEBUG
	mWebScraperModule = MakeRefCounted<AnonymousModule>("WebScraperD.dll");
#else
	mWebScraperModule = MakeRefCounted<AnonymousModule>("WebScraper.dll");
#endif
	mWebScraperModule->Init(KigsCore::Instance(), nullptr);

	mWebScraper = KigsCore::GetInstanceOf("theWebScrapper", "WebViewHandler");

	// can't find anonymous module ? don't use likes
	if (!mWebScraper->isSubType("WebViewHandler"))
	{
		mWebScraperModule->Close();
		mWebScraperModule = nullptr;
		mWebScraper = nullptr;
	}
	else
	{
		mWebScraper->Init();
		KigsCore::Connect(mWebScraper.get(), "navigationComplete", this, "LaunchScript");
		mWebScraper->setValue("Url", "https://twitter.com/login");
		KigsCore::Connect(mWebScraper.get(), "msgReceived", this, "treatWebScraperMessage");
		KigsCore::GetCoreApplication()->AddAutoUpdate(mWebScraper.get());

		ReadScripts();

#ifdef _DEBUG
		// give some time to user to login
		//mStartWaitQuota = GetApplicationTimer()->GetTime();
		//mWaitQuota = true;
#endif

	}

}

void	ScrapperManager::ReadScripts()
{

	u64 length;
	SP<CoreRawBuffer> rawbuffer = ModuleFileManager::LoadFileAsCharString("WalkerScript.txt", length, 1);
	if (rawbuffer)
	{
		mWalkInitScript = (const char*)rawbuffer->buffer();
		rawbuffer = nullptr;
	}

	rawbuffer = ModuleFileManager::LoadFileAsCharString("CallWalkerScript.txt", length, 1);
	if (rawbuffer)
	{
		mCallWalkScript = (const char*)rawbuffer->buffer();
		rawbuffer = nullptr;
	}
}

void	ScrapperManager::treatWebScraperMessage(CoreModifiable* sender, std::string msg)
{
	mLastActiveTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
	switch (mScraperState)
	{
	case  GET_LIKES:
	{
		if (msg == "") // something went wrong ? 
		{
			break;
		}
		if (msg.find("href") != std::string::npos)
		{
			size_t possibleUserPos = msg.find("twitter.com/");
			if (possibleUserPos != std::string::npos)
			{
				size_t endUserPos = msg.find("\"", possibleUserPos);
				std::string user = msg.substr(possibleUserPos + 12, endUserPos - (possibleUserPos + 12));
				if (user.find("search?") == std::string::npos) // seams OK
				{
					if (mCurrentScrappedUserNameList.size() && (mCurrentScrappedUserNameList.back().userName == user))
					{
						mCurrentScrappedUserNameList.back().foundCount++;
					}
					else
					{
						mCurrentScrappedUserNameList.push_back({ user,1 });
					}
				}
			}
		}
		else if (msg.find("scriptDone") != std::string::npos)
		{
			if (mCurrentScrappedUserNameList.size()) // some users were found ?
			{
				// get only valid ones
				std::vector<std::string>	validscrappedlist;
				for (const auto& u : mCurrentScrappedUserNameList)
				{
					if (u.foundCount >= 2)
					{
						validscrappedlist.push_back(u.userName);
					}
				}

				// check if the end of the list was found
				bool valid = true;
				if (mLastRetrievedLiker == validscrappedlist.back())
				{

					if (mListEndReachCount < 2) // try it again twice to be sure
					{
						mListEndReachCount++;
					}
					else
					{
						valid = false; // ok we reached the end of the list
					}
				}
				else
				{
					mLastRetrievedLiker = validscrappedlist.back();
					mListEndReachCount = 0;	
				}

				if (valid)
				{
					bool oneWasAdded = false;
					for (const auto& u : validscrappedlist)
					{
						auto f = mTweetLikersMap.find(u);

						if (f == mTweetLikersMap.end())
						{
							mTweetLikers.push_back(u);
							mTweetLikersMap[u] = 0;
							oneWasAdded = true;
						}
					}

					if (mTweetLikers.size() && ( oneWasAdded || (mListEndReachCount<2)))
					{
						//if (mTweetLikers.size() < 500) // if enough likers found
						{
							mCurrentScrappedUserNameList.clear();
							mNextScript = "var toscroll=document.querySelector('[href=\"/" + mTweetLikers.back() + "\"]');"\
								"toscroll.scrollIntoView(true);"\
								"setTimeout(function(){window.chrome.webview.postMessage(\"scriptDone\");},";
							if (mListEndReachCount)
							{
								// more delay
								mNextScript += "1000);";
							}
							else
							{
								// normal delay
								mNextScript += "300);";
							}
							LaunchScript(sender);
							mScraperState = SCROLL_LIKES;
						}
						
					}
					
				}
				
			}

			if (mScraperState != SCROLL_LIKES) // no more likers found
			{
				EmitSignal("LikersRetreived", mTweetLikers);
				mTweetLikers.clear();
				mTweetLikersMap.clear();
				mLastActiveTime = -1.0;
				mListEndReachCount = 0;
				mLastRetrievedLiker = "";
			}
		}
	}
	break;
	case SCROLL_LIKES:
	{
		mNextScript = mCallWalkScript;
		mScraperState = GET_LIKES;
		LaunchScript(sender);
	}
	break;
	}
}

void	ScrapperManager::LaunchScript(CoreModifiable* sender)
{
	if (mNextScript != "")
	{
		std::string currentURL = sender->getValue<std::string>("Url");
		if (currentURL != mCurrentURL)
		{
			// received a message from previous navigationCompleted
			// so do it again
			protectedLaunchScrap();
			return;
		}
		
		sender->setValue("Script", (UTF8Char*)mNextScript.c_str());
		mNextScript = "";
		mLastActiveTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
	}
}

void	ScrapperManager::protectedLaunchScrap()
{
	mNextScript = mWalkInitScript + mCallWalkScript;
	mScraperState = GET_LIKES;
	mWebScraper->setValue("Url", mCurrentURL);

	mLastActiveTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
}


void	ScrapperManager::launchScrap(const std::string& username, const std::string& tweetID)
{
	mCurrentURL = "https://twitter.com/" + username + "/status/" + tweetID + "/likes";
	protectedLaunchScrap();
}

double ScrapperManager::getInactiveTime()
{
	if (mLastActiveTime > 0.0)
	{
		double currentt = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
		return currentt - mLastActiveTime;
	}
}