#include <windows.h>
#include <inttypes.h>
#include "RTNetwork.h"
#include "FilePathManager.h"
#include "NotificationCenter.h"
#include "HTTPRequestModule.h"
#include "JSonFileParser.h"
#include "TinyImage.h"
#include "JPEGClass.h"
#include "PNGClass.h"
#include "GIFClass.h"
#include "TextureFileManager.h"
#include "UI/UIImage.h"
#include "AnonymousModule.h"
#include <iostream>

time_t     RTNetwork::mCurrentTime=0;
double		RTNetwork::mOldFileLimit = 0.0;

std::string	GetDate(int fromNowInSeconds)
{
	char yestDt[64];

	time_t now = time(NULL);
	now = now + fromNowInSeconds;
	struct tm* t = localtime(&now);

	sprintf(yestDt, "%d-%02d-%02dT",2000+t->tm_year-100,t->tm_mon+1, t->tm_mday);

	return yestDt;
}


template<typename T>
void	randomizeVector(std::vector<T>& v)
{
	unsigned int s = v.size();
	if (s>1)
	{
		unsigned int mod = s;
		for (unsigned int i = 0; i < (s - 1); i++)
		{
			unsigned int swapIndex = rand() % mod;

			T swap = v[mod - 1];
			v[mod - 1] = v[swapIndex];
			v[swapIndex] = swap;
			mod--;
		}
	}
}


IMPLEMENT_CLASS_INFO(RTNetwork);

IMPLEMENT_CONSTRUCTOR(RTNetwork)
{
	srand(time(NULL));
	mState.push_back(WAIT_STATE);
}

int getCreationYear(const std::string& created_date)
{
	char Day[6];
	char Mon[6];

	int	day_date;
	int	hours,minutes,seconds;
	int delta;
	int	year=0;

	sscanf(created_date.c_str(), "%s %s %d %d:%d:%d +%d %d", Day, Mon, &day_date, &hours, &minutes, &seconds,&delta ,&year);

	return year;
}


void	RTNetwork::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);


	mCurrentTime = time(0);

	SYSTEMTIME	retrieveYear;
	GetSystemTime(&retrieveYear);
	mCurrentYear = retrieveYear.wYear;

	// here don't use files olders than three months.
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;

	// Init App
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary("launchRTParams.json");

	// retreive parameters
	CoreItemSP foundBear;
	int bearIndex = 0;
	do
	{
		char	BearName[64];
		++bearIndex;
		sprintf(BearName, "TwitterBear%d", bearIndex);

		foundBear = initP[(std::string)BearName];

		if (!foundBear.isNil())
		{
			mTwitterBear.push_back("authorization: Bearer " + (std::string)foundBear);
			
		}
	} while (!foundBear.isNil());


	auto SetMemberFromParam = [&](auto& x, const char* id) {
		if (!initP[id].isNil()) x = initP[id];
	};

	SetMemberFromParam(mStartUserName, "UserName");
	SetMemberFromParam(mFromDate, "FromDate");
	SetMemberFromParam(mToDate, "ToDate");


	if ((mFromDate == "") || (mToDate == "") || (mStartUserName == ""))
	{
		mNeedExit = true;
		return;
	}

	int oldFileLimitInDays = 3 * 30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");

	mOldFileLimit = 60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays;

	SP<FilePathManager>& pathManager = KigsCore::Singleton<FilePathManager>();
	// when a path is given, search the file only with this path
	pathManager->setValue("StrictPath", true);
	pathManager->AddToPath(".", "json");

	CoreCreateModule(HTTPRequestModule, 0);


	// youtube video management
	SetMemberFromParam(mYoutubeKey, "GoogleKey");

	if (mYoutubeKey != "")
	{

		mYoutubeKey = std::string("&key=") + mYoutubeKey;

		// init google api connection
		mGoogleConnect = KigsCore::GetInstanceOf("googleConnect", "HTTPConnect");
		mGoogleConnect->setValue("HostName", "www.googleapis.com");
		mGoogleConnect->setValue("Type", "HTTPS");
		mGoogleConnect->setValue("Port", "443");
		mGoogleConnect->Init();
	}

	// init twitter connection
	mTwitterConnect = KigsCore::GetInstanceOf("TwitterConnect", "HTTPConnect");
	mTwitterConnect->setValue("HostName", "api.twitter.com");
	mTwitterConnect->setValue("Type", "HTTPS");
	mTwitterConnect->setValue("Port", "443");
	mTwitterConnect->Init();

	loadWebToAccountFile();
	// load anonymous module for web scraper
#ifdef _DEBUG
	mWebScraperModule = new AnonymousModule("WebScraperD.dll");
#else
	mWebScraperModule = new AnonymousModule("WebScraper.dll");
#endif
	mWebScraperModule->Init(KigsCore::Instance(), nullptr);

	mWebScraper = KigsCore::GetInstanceOf("theWebScrapper", "WebViewHandler");

	// can't find anonymous module ? don't use likes
	if (!mWebScraper->isSubType("WebViewHandler"))
	{
		mWebScraperModule->Close();
		delete mWebScraperModule;
		mWebScraper = nullptr;
		mWebScraperModule = nullptr;
	}
	else
	{
		mWebScraper->Init();
		mWebScraper->AddDynamicAttribute<maString,std::string>("encodedURL", "");
		KigsCore::Connect(mWebScraper.get(), "navigationComplete", this, "webscrapperNavigationComplete");
		AddAutoUpdate(mWebScraper.get());
	}

	std::string currentUserProgress = "Cache/UserNameRT/";
	currentUserProgress += mStartUserName + "_"+mFromDate+"_"+mToDate+".json";
	CoreItemSP currentP = LoadJSon(currentUserProgress,false,false);
	mState.back() = GET_USER_DETAILS;

	if (currentP.isNil()) // new user
	{
		std::string url = "1.1/users/show.json?screen_name=" + mStartUserName;
		mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
		mAnswer->AddHeader(mTwitterBear[NextBearer()]);
		mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
		mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", true);
		mState.push_back(WAIT_STARTUSERID);
		mAnswer->Init();
		myRequestCount++;
		RequestLaunched(1.1);
	}
	else // load current user
	{
		mStartUserID = currentP["id"];
		mCurrentTreatedAccount = getUser(mStartUserID);
		LoadUserStruct(mStartUserID, mCurrentTreatedAccount->mUserStruct, true);
	}

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
	mLastUpdate = GetApplicationTimer()->GetTime();
}

void	RTNetwork::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();


	if (mWaitQuota)
	{
		double dt = GetApplicationTimer()->GetTime() - mStartWaitQuota;
		// 2 minutes
		if (dt > (2.0 * 60.0))
		{

			mWaitQuota = false;
			mAnswer->GetRef(); 
			KigsCore::addAsyncRequest(mAnswer.get());
			mAnswer->Init();
			RequestLaunched(60.5);
		}
		
	}
	else 
	{
		switch (mState.back())
		{
		case WAIT_STATE: // wait

			break;
		case GET_USER_DETAILS:
		{

			if (!LoadUserStruct(mCurrentTreatedAccount->mID, mCurrentTreatedAccount->mUserStruct, true))
			{
				if (CanLaunchRequest())
				{
					std::string url = "1.1/users/show.json?user_id=" + std::to_string(mCurrentTreatedAccount->mID);
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", true);
					mAnswer->Init();
					myRequestCount++;
					mState.push_back(WAIT_STATE);
					RequestLaunched(1.1);
				}
			}
			else
			{
				mState.back() = GET_TWEETS;
			}
		}
		break;
		case GET_TWEETS:

		{
			if (mCurrentTreatedAccount->needMoreTweets())
			{
				if (CanLaunchRequest())
				{
					std::string url = "2/users/" + std::to_string(mCurrentTreatedAccount->mID) + "/tweets?tweet.fields=public_metrics,referenced_tweets&expansions=attachments.media_keys,attachments.poll_ids&media.fields=url";
					url += "&start_time=" + mFromDate + "T00:00:00Z";
					url += "&end_time=" + mToDate + "T23:59:59Z";

					url += "&max_results=100";
					if (mCurrentTreatedAccount->mNextTweetsToken != "-1")
					{
						url += "&pagination_token=" + mCurrentTreatedAccount->mNextTweetsToken;
					}
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getTweets", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->Init();
					myRequestCount++;
					mState.push_back(WAIT_STATE);
					RequestLaunched(0.5);
				}
			}
			else
			{
				CoreItemSP twts=mCurrentTreatedAccount->loadTweetsFile();
				mCurrentTreatedAccount->addTweets(twts, false);
				mCurrentTreatedAccount->updateEmbeddedURLList();
				mState.push_back(RESOLVE_URL);
			}
		}
		break;
		case RESOLVE_URL:
		{
			if (mCurrentTreatedAccount->mNeedURLs.size())
			{
				std::string askURL = "https://t.co/";
				askURL += mCurrentTreatedAccount->mNeedURLs.back();
				mWebScraper->setValue("encodedURL", mCurrentTreatedAccount->mNeedURLs.back());
				mWebScraper->setValue("Url", askURL);

				mState.push_back(WAIT_STATE);
			}
			else
			{
				mCurrentTreatedAccount->updateTweetRequestList();
				mState.back()=TREAT_QUOTED;
			}
		}
		break;
		case TREAT_QUOTED:
		{
			if (mCurrentTreatedAccount->mTweetToRequestList.size())
			{
				if (CanLaunchRequest())
				{
					std::string url = "2/tweets/" + std::to_string(mCurrentTreatedAccount->mTweetToRequestList.back()) + "?expansions=author_id&user.fields=name";
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getTweetAuthor", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->Init();
					myRequestCount++;
					mState.push_back(WAIT_STATE);
					RequestLaunched(3.5);
				}
			}
			else 
			{
				mCurrentTreatedAccount->updateRetweeterList();
				mState.back() = TREAT_RETWEETERS;
			}
		}
		break;
		case TREAT_RETWEETERS:
		{
			if (mCurrentTreatedAccount->mTweetToRequestList.size())
			{
				if (CanLaunchRequest())
				{
					std::string url = "1.1/statuses/retweeters/ids.json?id=" + std::to_string(mCurrentTreatedAccount->mTweetToRequestList.back()) + "&count=100";
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getRetweetAuthors", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->Init();
					myRequestCount++;
					mState.push_back(WAIT_STATE);
					RequestLaunched(3.5);
				}
			}
			else
			{
				
				mCurrentTreatedAccount->updateUserNameRequest();

				mState.back() = TREAT_USERNAME_REQUEST;

				if (mCurrentTreatedAccount->mYoutubeVideoList.size())
				{
					mState.push_back(GET_YOUTUBE_URL_CHANNEL);
				}

			}
		}
		break;

		case GET_YOUTUBE_URL_CHANNEL:
		{
			if (mCurrentTreatedAccount->mYoutubeVideoList.size())
			{
				std::string currentYTURL = *(mCurrentTreatedAccount->mYoutubeVideoList.begin());

				CoreItemSP guidfile = mCurrentTreatedAccount->loadYoutubeFile(currentYTURL);

				if (guidfile.isNil())
				{
					std::string url = "/youtube/v3/videos?part=snippet&id=";
					std::string request = url + currentYTURL + mYoutubeKey;
					mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelID", this);
					mAnswer->Init();
					mState.push_back(WAIT_STATE);
				}
				else
				{
					std::string ChannelGUID = guidfile["GUID"];
					std::string uname = getWebToTwitterUser(ChannelGUID);
					if (uname != "")
					{
						CoreItemSP currentUserJson = mCurrentTreatedAccount->loadUserID(uname);
						if (currentUserJson.isNil())
						{
							mCurrentTreatedAccount->mUserNameRequestList.insert(uname);
						}
						else
						{
							updateUserNameAndIDMaps(uname, currentUserJson["id"]);
						}
					}
					mCurrentTreatedAccount->mYoutubeVideoList.erase(mCurrentTreatedAccount->mYoutubeVideoList.begin());
				}
			}
			else
			{
				mState.pop_back();
			}
		}
		break;
		case TREAT_USERNAME_REQUEST:
		{
			if (mCurrentTreatedAccount->mUserNameRequestList.size())
			{
				if (CanLaunchRequest())
				{
					std::string url = "1.1/users/show.json?screen_name=" + *(mCurrentTreatedAccount->mUserNameRequestList.begin());
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", false);
					mAnswer->Init();
					myRequestCount++;
					mState.push_back(WAIT_STATE);
					RequestLaunched(1.1);
				}
			}
			else
			{
				
				if (mMainInterface.isNil() || mCurrentTreatedAccount->mUserStruct.mThumb.mTexture.isNil()) // wait main interface and thumb available 
				{
					break;
				}

				mCurrentTreatedAccount->updateStats();
				if (mAnalysedAccountList.size() == 0)
				{
					mCurrentTreatedAccount->setDepth(0);
					mCurrentTreatedAccount->setMandatory(true);
				}

				displayThumb* thmb=setupThumb(mCurrentTreatedAccount);
				setupLinks(thmb); // create links with already analysed accounts

				mAnalysedAccountList.push_back(mCurrentTreatedAccount);
				mCurrentTreatedAccount = chooseNextAccountToTreat();
				mState.pop_back();

				refreshShowed();

				if (mCurrentTreatedAccount)
				{
					mState.back() = GET_USER_DETAILS;
				}
				else
				{
					mState.back() = EVERYTHING_DONE;
				}
			}
		}
		break;

		} // switch end
	}
	// update graphics
	double dt = GetApplicationTimer()->GetTime() - mLastUpdate;
	if (dt > 1.0)
	{
		mLastUpdate = GetApplicationTimer()->GetTime();
		if (mMainInterface)
		{
			char textBuffer[256];
			sprintf(textBuffer, "Treated account : %d", mAnalysedAccountList.size());
			mMainInterface["TreatedAccount"]("Text") = textBuffer;
			
			mMainInterface["FromDate"]("Text") = mFromDate;		
			mMainInterface["ToDate"]("Text") = mToDate;

			if (mState.back() != EVERYTHING_DONE)
			{
				sprintf(textBuffer, "Twitter API requests : %d", myRequestCount);
				mMainInterface["RequestCount"]("Text") = textBuffer;

				if (mWaitQuota)
				{
					sprintf(textBuffer, "Wait quota count: %d", mWaitQuotaCount);
				}
				else
				{
					double requestWait = mNextRequestDelay - (mLastUpdate - mLastRequestTime);
					if (requestWait < 0.0)
					{
						requestWait = 0.0;
					}
					sprintf(textBuffer, "Next request in : %f", requestWait);
				}


				mMainInterface["RequestWait"]("Text") = textBuffer;

			}
			else
			{
				
				mMainInterface["RequestCount"]("Text") = "";
				mMainInterface["RequestWait"]("Text") = "";
			}
		}
		
	}

	moveThumbs();
	updateLinks();
}

void	RTNetwork::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
	clearUsers();
	mTwitterConnect = nullptr;
	mGoogleConnect = nullptr;
	mAnswer = nullptr;
}

void	RTNetwork::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
	}
}
void	RTNetwork::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = nullptr;	

	}
}

void	RTNetwork::refreshShowed()
{

}



TwitterAccount* RTNetwork::chooseNextAccountToTreat()
{
	// look up table for already done
	std::set<u64>									analysedAccount;
	for (auto a : mAnalysedAccountList)
	{
		analysedAccount.insert(a->mID);
	}

	// check for ex aequo (5% margin) in start account
	auto network = mAnalysedAccountList[0]->getSortedNetwork();

	u32 coefHas = network[0].second.first;
	u32 coefWas = network[0].second.second;


	for (const auto& b : network)
	{
		if (b.second.first > coefHas)
			coefHas = b.second.first;

		if (b.second.second > coefWas)
			coefWas = b.second.second;
	}


	u32 scoreHas = coefHas - (coefHas *5)/100;
	u32 scoreWas = coefWas - (coefWas * 5) / 100;
	for (auto n : network)
	{
		
		if ( (n.second.first >= scoreHas) || (n.second.second >= scoreWas))
		{
			if (analysedAccount.find(n.first) == analysedAccount.end())
			{
				TwitterAccount* user= getUser(n.first);
				user->setDepth(1);
				user->setMandatory(true);
				return user;
			}
		}
		
	}

	std::map<std::pair<u64, u64>, std::pair<u32, u32>>					matchedPair;
	std::vector<std::pair<u64, std::pair<u32,u32>>>		sortedFullV;

	// ID, ref count, score has, score was
	std::unordered_map<u64, std::pair<u32,std::pair<u32,u32>>>			coefs;
	
	// for each analysed account
	for (auto a : mAnalysedAccountList)
	{
		u64 aID = a->mID;
		// get network
		auto network = a->getSortedNetwork();
		for (auto n : network)
		{
			u64 nID = n.first;
			
			std::pair<u64, u64> idpair(aID,nID);
			if (aID > nID)
			{
				idpair.first = nID;
				idpair.second = aID;
			}

			if (coefs.find(nID) == coefs.end())
			{
				coefs.insert({ nID ,{ 1,n.second} });
			}
			else
			{
				coefs[nID].first++;
				coefs[nID].second.first += n.second.first;
				coefs[nID].second.second += n.second.second;
			}

			if (matchedPair.find(idpair) != matchedPair.end()) // was already matched
			{
				coefs[nID].first--;
				coefs[nID].second.first -= (n.second.first > matchedPair[idpair].first) ? matchedPair[idpair].first : n.second.first;
				coefs[nID].second.second -= (n.second.second > matchedPair[idpair].second) ? matchedPair[idpair].second : n.second.second;
			}
			else
			{
				matchedPair[idpair] = n.second;
			}
			
		}
	}

	if (coefs.size())
	{
		for (auto n : coefs)
		{
			if (n.second.first > 1) // at least referenced by 2 accounts
			{
				auto links = getValidLinks(n.first,false);
				if (links.size() > 1)
				{
					int bestd = 2;
					
					for (auto l : links)
					{
						if (l.second)
						{
							int d = l.first->mAccount->getDepth();
							if ((d >= 0) && (d < bestd)) // if valid link with depth 0 or 1
							{
								bestd = d;
							}
						}
					}

					if ( (bestd == 0) || ((bestd==1) && mAnalysedAccountList[0]->getLinkCoef(n.first)))// at least one direct link to main account
					{
						float score = ((n.second.second.first+ n.second.second.second) / n.second.first);
						score *= sqrtf(n.second.first);
						score /= (float)(bestd + 1);

						sortedFullV.push_back({ n.first,{(u32)score,(u32)bestd} });
					}
				}
				
			}
		}

		std::sort(sortedFullV.begin(), sortedFullV.end(), [](const std::pair<u64, std::pair<u32, u32>>& a1, const std::pair<u64, std::pair<u32, u32>>& a2)
			{
				if (a1.second.first == a2.second.first)
				{
					return a1.first > a2.first;
				}
				return (a1.second.first > a2.second.first);
			}
		);

		for (auto s : sortedFullV)
		{
			
			if (analysedAccount.find(s.first) == analysedAccount.end()) // check only not already done
			{
				TwitterAccount* user = getUser(s.first);
				user->setDepth(s.second.second+1);
				return user;
			}
		}
	}

	return nullptr;

}

CoreItemSP	RTNetwork::RetrieveJSON(CoreModifiable* sender)
{
	void* resultbuffer = nullptr;
	sender->getValue("ReceivedBuffer", resultbuffer);

	if (resultbuffer)
	{
		CoreRawBuffer* r = (CoreRawBuffer*)resultbuffer;
		std::string_view received(r->data(), r->size());

		std::string validstring(received);

		if (validstring.length() == 0)
		{
			
		}
		else
		{

			usString	utf8string((UTF8Char*)validstring.c_str());

			JSonFileParserUTF16 L_JsonParser;
			CoreItemSP result = L_JsonParser.Get_JsonDictionaryFromString(utf8string);

			if (!result["error"].isNil())
			{
				return nullptr;
			}
			if (!result["errors"].isNil())
			{
				if (!result["errors"][0]["code"].isNil())
				{
					int code = result["errors"][0]["code"];
					mApiErrorCode = code;
					if (code == 88)
					{
						mWaitQuota = true;
						mWaitQuotaCount++;
						mStartWaitQuota = GetApplicationTimer()->GetTime();
					}
				
					if (code == 32)
					{
						HTTPAsyncRequest* request = (HTTPAsyncRequest*)sender;
						mWaitQuota = true;
						mWaitQuotaCount++;
						mStartWaitQuota = GetApplicationTimer()->GetTime();
						request->ClearHeaders();
						int bearerIndex = request->getValue<int>("BearerIndex");
						// remove bearer from list
						mTwitterBear.erase(mTwitterBear.begin() + bearerIndex);
						mAnswer->AddHeader(mTwitterBear[NextBearer()]);
						mAnswer->setValue("BearerIndex", CurrentBearer());
					}
					
				}

				return nullptr;
			}
			
			return result;
			
		}
	}

	return nullptr;
}

void		RTNetwork::SaveJSon(const std::string& fname,const CoreItemSP& json, bool utf16)
{


	if (utf16)
	{
		JSonFileParserUTF16 L_JsonParser;
		L_JsonParser.Export((CoreMap<usString>*)json.get(), fname);
	}
	else
	{
		JSonFileParser L_JsonParser;
		L_JsonParser.Export((CoreMap<std::string>*)json.get(), fname);
	}

}

bool RTNetwork::checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle,double OldFileLimit)
{
	auto& pathManager = KigsCore::Singleton<FilePathManager>();
	filenamehandle = pathManager->FindFullName(fname);


	if ((filenamehandle->mStatus & FileHandle::Exist))
	{
		if (OldFileLimit > 1.0)
		{
			// Windows specific code 
#ifdef WIN32
			struct _stat resultbuf;

			if (_stat(filenamehandle->mFullFileName.c_str(), &resultbuf) == 0)
			{
				auto mod_time = resultbuf.st_mtime;

				double diffseconds = difftime(mCurrentTime, mod_time);

				if (diffseconds > OldFileLimit)
				{
					return false;
				}
			}
#endif
		}
	
		return true;
		
	}


	return false;
}

CoreItemSP	RTNetwork::LoadJSon(const std::string& fname, bool utf16, bool checkfileDate)
{
	SmartPointer<::FileHandle> filenamehandle;

	CoreItemSP initP(nullptr);
	if (checkValidFile(fname, filenamehandle, checkfileDate?mOldFileLimit:0.0))
	{
		if (utf16)
		{
			JSonFileParserUTF16 L_JsonParser;
			initP = L_JsonParser.Get_JsonDictionary(filenamehandle);
		}
		else
		{
			JSonFileParser L_JsonParser;
			initP = L_JsonParser.Get_JsonDictionary(filenamehandle);
		}
	}
	return initP;
}



DEFINE_METHOD(RTNetwork, getTweets)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		mCurrentTreatedAccount->addTweets(json,true);
		mState.pop_back();
		
	}
	
	return true;
}

void	RTNetwork::updateUserNameAndIDMaps(const std::string& uname,u64 uid)
{
	if (mUserIDToNameMap.find(uid) == mUserIDToNameMap.end())
	{
		mUserIDToNameMap[uid] = uname;
	}

	if (mNameToUserIDMap.find(uname) == mNameToUserIDMap.end())
	{
		mNameToUserIDMap[uname] = uid;
	}
}


DEFINE_METHOD(RTNetwork, getTweetAuthor)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{

		CoreItemSP name = json["includes"]["users"][0]["username"];
		CoreItemSP id = json["includes"]["users"][0]["id"];
		if (!name.isNil() && !id.isNil())
		{
			// check if user exists
			CoreItemSP useridfile = mCurrentTreatedAccount->loadUserID(name);
			if (useridfile.isNil())
			{
				mCurrentTreatedAccount->saveUserID(name, id);
			}

			mCurrentTreatedAccount->saveTweetUserFile(mCurrentTreatedAccount->mTweetToRequestList.back(), name);
			mCurrentTreatedAccount->mTweetToRequestList.pop_back();

			updateUserNameAndIDMaps(name, id);
		}
		else
		{
			// author not available, just save empty author
			mCurrentTreatedAccount->saveTweetUserFile(mCurrentTreatedAccount->mTweetToRequestList.back(), "");
			mCurrentTreatedAccount->mTweetToRequestList.pop_back();
		}

		mState.pop_back();
	}
	
	return true;
}

DEFINE_METHOD(RTNetwork, getRetweetAuthors)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		mCurrentTreatedAccount->saveRetweeters(mCurrentTreatedAccount->mTweetToRequestList.back(), json);
		mCurrentTreatedAccount->mTweetToRequestList.pop_back();

		mState.pop_back();
	}

	return true;
}



DEFINE_METHOD(RTNetwork, getChannelID)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		CoreItemSP snippet = json["items"][0]["snippet"];

		std::string channelID = "";
		if (!snippet.isNil())
		{
			CoreItemSP ChannelID = snippet["channelId"];
			CoreItemSP ChannelTitle = snippet["channelTitle"];
			channelID = ChannelID;
			std::string account;
			if (mWebToTwitterMap.find(ChannelID) == mWebToTwitterMap.end())
			{
				std::cout << "Enter account for Youtube Channel : " << (std::string)ChannelTitle << std::endl;

				
				std::cin >> account;
				if (account.length() < 4)
					account = "";

				mWebToTwitterMap[ChannelID] = account;

				saveWebToAccountFile();
			}

			if (account!="")
			{
				CoreItemSP currentUserJson = mCurrentTreatedAccount->loadUserID(account);
				if (currentUserJson.isNil())
				{
					mCurrentTreatedAccount->mUserNameRequestList.insert(account);
				}
				else
				{
					updateUserNameAndIDMaps(account, currentUserJson["id"]);
				}
			}
		}

		mCurrentTreatedAccount->saveYoutubeFile(*(mCurrentTreatedAccount->mYoutubeVideoList.begin()), channelID);
		mCurrentTreatedAccount->mYoutubeVideoList.erase(mCurrentTreatedAccount->mYoutubeVideoList.begin());

		mState.pop_back();
	}
	
	return true;
}


DEFINE_METHOD(RTNetwork, getUserDetails)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{

		u64			currentID= json["id"];

		if ((mState.size() > 1) && (mState[mState.size() - 2] == TREAT_USERNAME_REQUEST)) // requested ID from name
		{
			mCurrentTreatedAccount->saveUserID(*(mCurrentTreatedAccount->mUserNameRequestList.begin()), currentID);
			mCurrentTreatedAccount->mUserNameRequestList.erase(mCurrentTreatedAccount->mUserNameRequestList.begin());
		}

		std::string user = json["screen_name"];

		updateUserNameAndIDMaps(user, currentID);

		TwitterAccount* current=getUser(currentID);

		if (mCurrentTreatedAccount == nullptr) // start account ?
		{
			mCurrentTreatedAccount = current;
		}

		current->mUserStruct.mName = user;
		current->mID = currentID;
		current->mUserStruct.mFollowersCount = json["followers_count"];
		current->mUserStruct.mFollowingCount = json["friends_count"];
		current->mUserStruct.mStatuses_count = json["statuses_count"];
		current->mUserStruct.UTCTime = json["created_at"];
		current->mUserStruct.mThumb.mURL = CleanURL(json["profile_image_url_https"]);

		SaveUserStruct(currentID, current->mUserStruct);

		bool requestThumb;
		if (sender->getValue("RequestThumb", requestThumb))
		{
			if (requestThumb)
				if (!LoadThumbnail(currentID, current->mUserStruct))
				{
					LaunchDownloader(currentID, current->mUserStruct);
				}
		}


		if (mState.back() == WAIT_STARTUSERID)
		{
			mStartUserID = currentID;
			JSonFileParser L_JsonParser;
			CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
			CoreItemSP idP = CoreItemSP::getCoreItemOfType<CoreValue<u64>>();
			idP = currentID;
			initP->set("id", idP);
			std::string filename = "Cache/UserNameRT/" + mStartUserName + "_" + mFromDate + "_" + mToDate + ".json";
			L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
		}

		mState.pop_back();
		
	}
	else if(!mWaitQuota)
	{
		if ((mState.size() > 1) && (mState[mState.size() - 2] == TREAT_USERNAME_REQUEST)) // user name doesn't exist anymore
		{
			mCurrentTreatedAccount->saveUserID(*(mCurrentTreatedAccount->mUserNameRequestList.begin()), 0);
			updateUserNameAndIDMaps(*(mCurrentTreatedAccount->mUserNameRequestList.begin()), 0);
			mCurrentTreatedAccount->mUserNameRequestList.erase(mCurrentTreatedAccount->mUserNameRequestList.begin());
			mState.pop_back();
		}

		if (mApiErrorCode == 63) // user suspended
		{
			
		}

	}

	return true;
}

void ReplaceStr(std::string& str,
	const std::string& oldStr,
	const std::string& newStr)
{
	std::string::size_type pos = 0u;
	while ((pos = str.find(oldStr, pos)) != std::string::npos) 
	{
		str.replace(pos, oldStr.length(), newStr);
		pos += newStr.length();
	}
}

void		RTNetwork::LaunchDownloader(u64 id, TwitterAccount::UserStruct& ch)
{
	// first, check that downloader for the same thumb is not already launched

	for (const auto& d : mDownloaderList)
	{
		if (d.second.first == id) // downloader found
		{
			return;
		}
	}

	if (ch.mThumb.mURL == "") // need a valid url
	{
		return;
	}

	std::string biggerThumb = ch.mThumb.mURL;
	ReplaceStr(biggerThumb, "_normal", "_bigger");

	CMSP CurrentDownloader = KigsCore::GetInstanceOf("downloader", "ResourceDownloader");
	CurrentDownloader->setValue("URL", biggerThumb);
	KigsCore::Connect(CurrentDownloader.get(), "onDownloadDone", this, "thumbnailReceived");

	mDownloaderList.push_back({ CurrentDownloader , {id,&ch} });

	CurrentDownloader->Init();
}

std::string  RTNetwork::CleanURL(const std::string& url)
{
	std::string result = url;
	size_t pos;

	do
	{
		pos = result.find("\\/");
		if (pos != std::string::npos)
		{
			result = result.replace(pos, 2, "/", 1);
		}

	} while (pos != std::string::npos);

	return result;
}

void	RTNetwork::thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader)
{

	// search used downloader in vector
	std::vector<std::pair<CMSP, std::pair<u64, TwitterAccount::UserStruct*>> > tmpList;

	for (const auto& p : mDownloaderList)
	{
		if (p.first.get() == downloader)
		{

			std::string	url = downloader->getValue<std::string>("URL");
			std::string::size_type pos=	url.rfind('.');
			std::string ext = url.substr(pos);
			TinyImage* img = nullptr;

			if (ext == ".png")
			{
				img = new PNGClass(data);
			}
			else if (ext == ".gif")
			{
				img = new GIFClass(data);
			}
			else
			{
				img = new JPEGClass(data);
				ext = ".jpg";
			}
			if (img->IsOK())
			{
				TwitterAccount::UserStruct* toFill = p.second.second;

				std::string filename = "Cache/Thumbs/";
				filename += GetIDString(p.second.first);
				filename += ext;

				// export image
				SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
				if (L_File->mFile)
				{
					Platform_fwrite(data->buffer(), 1, data->length(), L_File.get());
					Platform_fclose(L_File.get());
				}

				toFill->mThumb.mTexture = KigsCore::GetInstanceOf((std::string)toFill->mName + "tex", "Texture");
				toFill->mThumb.mTexture->Init();

				SmartPointer<TinyImage>	imgsp = OwningRawPtrToSmartPtr(img);
				toFill->mThumb.mTexture->CreateFromImage(imgsp);
			}
			else
			{
				// thumb has probably changed, the user file have to be removed
				u64 id = p.second.first;
				std::string folderName = GetUserFolderFromID(id);

				std::string filename = "Cache/Users/";
				filename += folderName + "/" + GetIDString(id) + ".json";

				ModuleFileManager::RemoveFile(filename.c_str());


				delete img;
			}
		}
		else
		{
			tmpList.push_back(p);
		}
	}
	mDownloaderList = std::move(tmpList);
}

std::string	RTNetwork::GetUserFolderFromID(u64 id)
{
	std::string result;

	for (int i = 0; i < 4; i++)
	{
		result +='0'+(id % 10);
		id = id / 10;
	}

	return result;
}

std::string	RTNetwork::GetIDString(u64 id)
{
	char	idstr[64];
	sprintf(idstr,"%llu", id);

	return idstr;
}

// load json channel file dans fill ChannelStruct
bool		RTNetwork::LoadUserStruct(u64 id, TwitterAccount::UserStruct& ch, bool requestThumb)
{
	std::string filename = "Cache/Users/";

	std::string folderName = GetUserFolderFromID(id);

	filename += folderName + "/" + GetIDString(id) + ".json";

	CoreItemSP initP = LoadJSon(filename, true);

	if (initP.isNil()) // file not found, return
	{
		return false;
	}

	ch.mName = initP["Name"];
	ch.mFollowersCount = initP["FollowersCount"];
	ch.mFollowingCount = initP["FollowingCount"];
	ch.mStatuses_count = initP["StatusesCount"];
	ch.UTCTime = initP["CreatedAt"];
	if (!initP["ImgURL"].isNil())
	{
		ch.mThumb.mURL = initP["ImgURL"];
	}
	if (requestThumb && ch.mThumb.mTexture.isNil())
	{
		if (!LoadThumbnail(id, ch))
		{
			if (ch.mThumb.mURL != "")
			{
				LaunchDownloader(id, ch);
			}
		}
	}
	return true;
}


void		RTNetwork::loadWebToAccountFile()
{
	mWebToTwitterMap.clear();
	std::string filename = "Cache/WebToAccount.json";

	CoreItemSP initP = LoadJSon(filename, false,false);

	if (initP.isNil()) // file not found, return
	{
		return;
	}

	CoreItemIterator it = initP.begin();
	while(it != initP.end())
	{
		std::string value;
		std::string key;

		CoreItemSP val = (*it);
		val->getValue(value);

		it.getKey(key);
		
		mWebToTwitterMap.insert({ key,value });

		it++;
	}
}
void		RTNetwork::saveWebToAccountFile()
{
	JSonFileParser L_JsonParser;
	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
	
	for (auto m : mWebToTwitterMap)
	{
		initP->set(m.first, CoreItemSP::getCoreValue(m.second));
	}

	std::string filename = "Cache/WebToAccount.json";

	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
}

void		RTNetwork::SaveUserStruct(u64 id, TwitterAccount::UserStruct& ch)
{

	JSonFileParserUTF16 L_JsonParser;
	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<usString>>();
	initP->set("Name", CoreItemSP::getCoreValue(ch.mName));
	initP->set("FollowersCount", CoreItemSP::getCoreValue((int)ch.mFollowersCount));
	initP->set("FollowingCount", CoreItemSP::getCoreValue((int)ch.mFollowingCount));
	initP->set("StatusesCount", CoreItemSP::getCoreValue((int)ch.mStatuses_count));
	initP->set("CreatedAt", CoreItemSP::getCoreValue((std::string)ch.UTCTime));

	if (ch.mThumb.mURL != "")
	{
		initP->set("ImgURL", CoreItemSP::getCoreValue((usString)ch.mThumb.mURL));
	}

	std::string folderName = GetUserFolderFromID(id);

	std::string filename = "Cache/Users/";
	filename += folderName + "/" + GetIDString(id) + ".json";

	L_JsonParser.Export((CoreMap<usString>*)initP.get(), filename);
}

bool		RTNetwork::LoadThumbnail(u64 id, TwitterAccount::UserStruct& ch)
{
	SmartPointer<::FileHandle> fullfilenamehandle;

	std::vector<std::string> ExtensionList = {".jpg",".png" ,".gif" };

	for (const auto& xt : ExtensionList)
	{
		std::string filename = "Cache/Thumbs/";
		filename += GetIDString(id);
		filename += xt;

		if (checkValidFile(filename, fullfilenamehandle, mOldFileLimit))
		{
			auto& textureManager = KigsCore::Singleton<TextureFileManager>();
			ch.mThumb.mTexture = textureManager->GetTexture(filename);
			return true;
		}
	}
	return false;
}


std::vector<u64>		RTNetwork::LoadIDVectorFile(const std::string& filename)
{
	std::vector<u64> loaded;
	if (LoadDataFile<u64>(filename, loaded))
	{
		if (loaded.size() == 0) // following unavailable
		{
			loaded.push_back(mStartUserID);
		}
	}

	return loaded;
}

v3f RTNetwork::getRandomColor()
{

	auto normalizeRand = []()->float
	{
		float fresult = ((float)(rand() % 1001)) / 1000.0f;
		return fresult;
	};


	v3f schemas[6] = { {1.0,0.0,0.0},{0.0,1.0,0.0},{0.0,0.0,1.0},{1.0,1.0,0.0},{1.0,0.0,1.0},{0.0,0.0,0.0} };

	v3f result;
	bool foundColorOK = true;
	
	int countLoop = 0;

	do
	{
		result = schemas[rand() % 6];

		for (size_t i = 0; i < 3; i++)
		{
			if (result[i] > 0.5f)
			{
				result[i] = 0.6f + normalizeRand() * 0.4f;
			}
			else
			{
				result[i] = normalizeRand() * 0.4f;
			}
		}

		foundColorOK = true;

		for (auto c : mAlreadyFoundColors)
		{
			v3f diff(c - result);
			if (Norm(diff) < 0.15f)
			{
				foundColorOK = false;
				break;
			}
		}



		countLoop++;

	} while ((!foundColorOK)&&(countLoop<8));


	mAlreadyFoundColors.push_back(result);

	return result;

}

RTNetwork::displayThumb* RTNetwork::setupThumb(TwitterAccount* account)
{
	displayThumb& newThumb=*(new displayThumb());
	newThumb.mAccount = account;
	newThumb.mLink.clear();
	newThumb.mName = account->mUserStruct.mName.ToString();

	std::string thumbName = newThumb.mName;

	newThumb.mThumb = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);

	TwitterAccount* mainAccount = account;
	if (mAnalysedAccountList.size())
	{
		mainAccount = mAnalysedAccountList[0];
	}

	float mainRadius = logf(mainAccount->mUserStruct.mFollowersCount);
	float radiuscoef = 1.5f*logf(account->mUserStruct.mFollowersCount)/ mainRadius;
	
	if (radiuscoef < 0.4f)
	{
		radiuscoef = 0.4f;
	}
	else if (radiuscoef > 2.5f)
	{
		radiuscoef = 2.5f;
	}

	newThumb.mRadius = 39.0f * radiuscoef;

	newThumb.mThumb->setValue("PreScaleX", radiuscoef);
	newThumb.mThumb->setValue("PreScaleY", radiuscoef);

	newThumb.mThumb["ChannelName"]->setValue("PreScaleX", 1.0f/radiuscoef);
	newThumb.mThumb["ChannelName"]->setValue("PreScaleY", 1.0f/radiuscoef);
	newThumb.mThumb["ChannelPercent"]->setValue("PreScaleX", 1.0f / radiuscoef);
	newThumb.mThumb["ChannelPercent"]->setValue("PreScaleY", 1.0f / radiuscoef);

	if (mainAccount == account)
		newThumb.mBackColor.Set(0.368f, 0.663f, 0.8666f);
	else
		newThumb.mBackColor = getRandomColor();

	newThumb.mThumb["thumbbackground"]("Color") = newThumb.mBackColor;

	v2f pos(0.1f + (rand() % 800) * 0.001f, 0.1f + (rand() % 800) * 0.001f);

	newThumb.mPos.Set(pos.x * 1280.0f, pos.y * 800.0f);
	newThumb.mThumb->setValue("Dock", pos);
	newThumb.mThumb["ChannelName"]("Text") = account->mUserStruct.mName;
	newThumb.mThumb["ChannelPercent"]("Text") = std::to_string((int)(account->mSourceCoef*100.0f)) + "\%";
	newThumb.mThumb->addItem(account->mUserStruct.mThumb.mTexture);
	mMainInterface["background"]->addItem(newThumb.mThumb);

	mThumbs.push_back(&newThumb);

	return mThumbs.back();
}


std::vector<std::pair<RTNetwork::displayThumb*, u32>> RTNetwork::getValidLinks(u64 uID, bool includeEmpty)
{
	
	std::vector<std::pair<displayThumb*, u32>> links;
	for (auto a : mThumbs)
	{
		if (a->mAccount->mID != uID)
		{
			u32 totalcoef = a->mAccount->getConnectionCount();
			u32 totalcoef_pc[2] = { (totalcoef * 1) / 100,(totalcoef * 5) / 100 };
			int d = a->mAccount->getDepth();
			if ((d >= 0) && (d < 2))
			{
				u32 lcoef = a->mAccount->getLinkCoef(uID);
				if (lcoef > totalcoef_pc[d])
				{
					links.push_back({ a,lcoef });
				}
				else
				{
					if(includeEmpty)
						links.push_back({ a, 0 });
				}
			}
			else
			{
				if (includeEmpty)
					links.push_back({ a, 0 });
			}
		}
	}

	if (links.size())
	{
		std::sort(links.begin(), links.end(), [](const std::pair<displayThumb*, u32>& a1, const std::pair<displayThumb*, u32>& a2)
			{
				if (a1.second == a2.second)
				{
					return a1.first > a2.first;
				}
				return (a1.second > a2.second);
			}
		);
	}
	return links;
}

void	RTNetwork::setupLinks(displayThumb* account)
{

	auto normalizeRand = []()->float
	{
		float result = ((float)(rand() % 1001)) / 1000.0f;
		return result;
	};

	u64 nID = account->mAccount->mID;

	std::vector<std::pair<displayThumb*, u32>> links = getValidLinks(nID,true);
	
	if (links.size())
	{
	
		for (auto l : links)
		{
			thumbLink toAdd;
			toAdd.mThumbs[0] = l.first;
			toAdd.mThumbs[1] = account;
			toAdd.mLength = l.second;

			u64 oID = l.first->mAccount->mID;

			std::pair<u64, u64> k(nID, oID);
			if (nID > oID)
			{
				k.first = oID;
				k.second = nID;
			}

			if (toAdd.mLength > 0.0f)
			{
				toAdd.mDisplayedLink = KigsCore::GetInstanceOf("link", "UIPanel");
				toAdd.mDisplayedLink->setValue("SizeX", 50);
				toAdd.mDisplayedLink->setValue("SizeY", sqrtf(toAdd.mLength));
				toAdd.mDisplayedLink->setValue("Priority", 5);
				toAdd.mDisplayedLink->setValue("Anchor", v2f(0.0,0.5));
				toAdd.mDisplayedLink->setValue("Dock", v2f(0.0, 0.0));

				v3f randcolor(0.5f + normalizeRand() * 0.5, normalizeRand() * 0.6, normalizeRand() * 0.6);

				toAdd.mDisplayedLink->setValue("Color", randcolor);
				mMainInterface["background"]->addItem(toAdd.mDisplayedLink);
			}

			mLinks[k] = toAdd;

			account->mLink.push_back(&mLinks[k]);
			l.first->mLink.push_back(&mLinks[k]);
		}

	}	

}


void					RTNetwork::SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename)
{
	SaveDataFile<u64>(filename, v);
}

void	RTNetwork::moveThumbs()
{

	if (mThumbs.size() < 2)
		return;

	const float dt = 0.1f;
	std::vector<v2f>	forces;
	forces.resize(mThumbs.size());

	for (auto& f : forces)
	{
		f.Set(0.0f, 0.0f);
	}

	size_t findex = 0;
	for (auto t : mThumbs)
	{
		// compute forces
		for (auto& l : t->mLink)
		{
			
			displayThumb* other = l->mThumbs[0];
			if (other == t)
			{
				other = l->mThumbs[1];
			}
			v2f posdiff = other->mPos;
			posdiff -= t->mPos;

			float dist = Norm(posdiff);
			posdiff.Normalize();

			if (l->mLength > 0.0f) // spring
			{
				float llen = t->mRadius + other->mRadius + 30.0f + 5000.0f / (10.0f + l->mLength);
				forces[findex] += posdiff * (dist - llen) * 0.01f;
			}
			else // just repel
			{
				forces[findex] -= posdiff * 10000.0f / (dist * dist);
			}
		}

		// friction
		forces[findex] -= t->mSpeed * 0.1f;

		findex++;
	}

	v2f bary(0.0f,0.0f);
	findex = 0;
	for (auto& t : mThumbs)
	{
		t->mAcceleration = forces[findex];
		if (Norm(t->mAcceleration) > 10.0f)
		{
			t->mAcceleration.Normalize();
			t->mAcceleration*=10.0f;
		}
		t->mSpeed += t->mAcceleration * dt;
		t->mPos += t->mSpeed * dt;
		bary += t->mPos;
		findex++;
	}

	bary /= (float)mThumbs.size();

	bary -= {1280.0f / 2.0f, 800.0f / 2.0f};

	for (auto t : mThumbs)
	{
		t->mPos -= bary;
		t->mThumb->setValue("Dock", v2f(t->mPos.x/1280.0f, t->mPos.y/800.0f));
	}

}

void	RTNetwork::updateLinks()
{
	for (auto l : mLinks)
	{
		if (!l.second.mDisplayedLink.isNil())
		{

			v2f p1 = l.second.mThumbs[0]->mPos;
			v2f p2 = l.second.mThumbs[1]->mPos;

			v2f dp(p2 - p1);
			float dist = Norm(dp);

			float rotate = atan2f(dp.y, dp.x);

			l.second.mDisplayedLink->setValue("SizeX", dist);
			l.second.mDisplayedLink->setValue("RotationAngle", rotate);
			l.second.mDisplayedLink->setValue("Dock", v2f(l.second.mThumbs[0]->mPos.x / 1280.0f, l.second.mThumbs[0]->mPos.y / 800.0f) );
		}
	}
}


void	RTNetwork::webscrapperNavigationComplete(CoreModifiable* sender)
{
	std::string geturl = sender->getValue<std::string>("Url");

	if (geturl.find("https://t.co") != std::string::npos) // not decoded
	{
		return;
	}

	std::string encoded = sender->getValue<std::string>("encodedURL");

	if ((encoded != "") && (mCurrentTreatedAccount->mNeedURLs.back() == encoded))
	{
		sender->setValue("encodedURL","");
		mCurrentTreatedAccount->saveURL(mCurrentTreatedAccount->mNeedURLs.back(), geturl);
		mCurrentTreatedAccount->mNeedURLs.pop_back();
		mState.pop_back();
	}
}

std::string	RTNetwork::getTwitterAccountForURL(const std::string& url)
{
	std::string hostname;

	size_t pos=url.find(':');
	if (pos != std::string::npos)
	{
		size_t endpos= url.find('/',pos+3);

		hostname = url.substr(pos+3, endpos - (pos + 3));

		// first end dot
		size_t dotpos = hostname.rfind('.');
		// second end dot
		dotpos=hostname.rfind('.', dotpos-1);

		if (dotpos != std::string::npos)
		{
			hostname = hostname.substr(dotpos + 1);
		}

		// special case for YouTube 

		if (hostname.find("youtube") != std::string::npos)
		{
			size_t watchpos = url.find("/watch?v=");
			if (watchpos != std::string::npos)
			{
				size_t	endpos = url.find("&", watchpos+9);
				size_t lenght = std::string::npos;
				if (endpos != std::string::npos)
				{
					lenght = endpos - (watchpos + 9);
				}
				
				return std::string("YT:")+url.substr(watchpos + 9, lenght);
				
			}
			return "";
		}
		// special case for Twitter
		if (hostname.find("twitter") != std::string::npos)
		{
			size_t statuspos = url.find("/status");
			if (statuspos != std::string::npos)
			{
				size_t	userpos= url.rfind("/", statuspos-1);
				if (userpos != std::string::npos)
				{
					return url.substr(userpos+1, statuspos-userpos-1);
				}
			}
			return "";
		}
		if (mWebToTwitterMap.find(hostname) != mWebToTwitterMap.end())
		{
			return mWebToTwitterMap[hostname];
		}

		std::cout << "Enter account for url : " << hostname << std::endl;
	
		std::string account;
		std::cin >> account;
		if (account.length() < 3)
			account = "";

		mWebToTwitterMap[hostname] = account;

		saveWebToAccountFile();

		return account;
	}

	return "";
}