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
#include "ModuleInput.h"
#include <iostream>
#include <iomanip> 

time_t     RTNetwork::mCurrentTime=0;
double		RTNetwork::mOldFileLimit = 0.0;


time_t	getDateAndTime(const std::string& d)
{
	std::tm gettm;
	memset(&gettm, 0, sizeof(gettm));
	std::istringstream ss(d);
	ss >> std::get_time(&gettm, "%Y-%m-%dT%H:%M:%S.000Z");
	return mktime(&gettm);
}

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

	time_t starttime = getDateAndTime(mFromDate + "T00:00:00.000Z");
	time_t endtime = getDateAndTime(mToDate + "T00:00:00.000Z");

	double diffseconds = difftime(endtime, starttime);

	mDurationInDays = (u32)(diffseconds / (3600.0 * 24.0));
	mDurationInDays++;

	if ( (mDurationInDays < 2) || (diffseconds<0.0))
	{
		mNeedExit = true;
		return;
	}

	int oldFileLimitInDays = 3 * 30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");

	mOldFileLimit = 60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays;

	SP<FilePathManager> pathManager = KigsCore::Singleton<FilePathManager>();
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

	CoreItemSP currentP = TwitterAccount::loadUserID(mStartUserName);
	mState.back() = GET_USER_DETAILS;

	if (currentP.isNil()) // new user
	{
		std::string url = "2/users/by/username/" + mStartUserName + "?user.fields=created_at,public_metrics,profile_image_url";
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
					std::string url = "2/users/" + std::to_string(mCurrentTreatedAccount->mID) + "?user.fields=created_at,public_metrics,profile_image_url";
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
					std::string url = "2/users/by/username/" + *(mCurrentTreatedAccount->mUserNameRequestList.begin()) + "?user.fields=created_at,public_metrics,profile_image_url";
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
				if (mCurrentTreatedAccount->needAddToNetwork())
				{
					setupLinks(thmb); // create links with already analysed accounts
				}

				addDisplayThumb(thmb);

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
		
		if ( (scoreHas && (n.second.first >= scoreHas)) || (scoreWas && (n.second.second >= scoreWas)))
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
	std::vector<std::pair<u64, std::pair<u32,u32>>>						sortedFullV;

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
				bool stronglink = false;
				auto links = getValidLinks(n.first, stronglink,false);
				if(links.size())
				{
					int bestd = 2;
					
					for (auto l : links)
					{
						if (l.second.first+ l.second.second)
						{
							int d = l.first->mAccount->getDepth();
							if ((d >= 0) && (d < bestd)) // if valid link with depth 0 or 1
							{
								bestd = d;
							}
						}
					}

					bool can_add = false;
					if ((bestd <=1) && mAnalysedAccountList[0]->getLinkCoef(n.first) && (links.size() > 1))
					{
						can_add = true;
					}
					else if ( ( bestd == 0 ) && (stronglink))
					{
						can_add = true;
					}


					if (can_add)// at least one direct link to main account
					{
						float score = ((n.second.second.first+ (n.second.second.second>>2)) / n.second.first);
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
	auto pathManager = KigsCore::Singleton<FilePathManager>();
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
		CoreItemSP	data = json["data"];
		CoreItemSP	public_m = data["public_metrics"];
		u64			currentID= data["id"];

		if ( ((mState.size() > 1) && (mState[mState.size() - 2] == TREAT_USERNAME_REQUEST)) ||
			(mState.back() == WAIT_STARTUSERID) )// requested ID from name
		{
			mCurrentTreatedAccount->saveUserID(*(mCurrentTreatedAccount->mUserNameRequestList.begin()), currentID);
			mCurrentTreatedAccount->mUserNameRequestList.erase(mCurrentTreatedAccount->mUserNameRequestList.begin());
		}

		std::string user = data["username"];

		updateUserNameAndIDMaps(user, currentID);

		TwitterAccount* current=getUser(currentID);

		if (mCurrentTreatedAccount == nullptr) // start account ?
		{
			mCurrentTreatedAccount = current;
		}

		current->mUserStruct.mName = user;
		current->mID = currentID;
		current->mUserStruct.mFollowersCount = public_m["followers_count"];
		current->mUserStruct.mFollowingCount = public_m["following_count"];
		current->mUserStruct.mStatuses_count = public_m["tweet_count"];
		current->mUserStruct.UTCTime = data["created_at"];
		current->mUserStruct.mThumb.mURL = CleanURL(data["profile_image_url"]);

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
			auto textureManager = KigsCore::Singleton<TextureFileManager>();
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
			if (Norm(diff) < 0.25f)
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

	newThumb.mThumb->setValue("PreScale", v2f(radiuscoef, radiuscoef));

	newThumb.mThumb["ChannelName"]->setValue("PreScale", v2f(1.0f/radiuscoef, 1.0f / radiuscoef));
	newThumb.mThumb["HasRetweet"]->setValue("PreScale", v2f(1.0f / radiuscoef, 1.0f / radiuscoef));
	newThumb.mThumb["TweetCount"]->setValue("PreScale", v2f(1.0f / radiuscoef, 1.0f / radiuscoef));

	if (mainAccount == account)
		newThumb.mBackColor.Set(0.368f, 0.663f, 0.8666f);
	else
		newThumb.mBackColor = getRandomColor();

	newThumb.mThumb["thumbbackground"]("Color") = newThumb.mBackColor;

	v2f pos(0.1f + (rand() % 800) * 0.001f, 0.1f + (rand() % 800) * 0.001f);

	newThumb.mPos.Set(pos.x * 1920.0f, pos.y * 1080.0f);
	newThumb.mThumb->setValue("Dock", pos);
	newThumb.mThumb["ChannelName"]("Text") = account->mUserStruct.mName;
	newThumb.mThumb["HasRetweet"]("Text") = "RT:" + std::to_string(account->getHasRTCount());
	newThumb.mThumb["TweetCount"]("Text") = "Twt:" + std::to_string(account->getTweetCount());
	newThumb.mThumb->addItem(account->mUserStruct.mThumb.mTexture);

	if (mainAccount == account)
	{
		account->setNeedAddToNetwork(true);
	}

	

	
	mThumbs.push_back(&newThumb);

	return mThumbs.back();
}

void	RTNetwork::addDisplayThumb(RTNetwork::displayThumb* thmb)
{
	if (thmb->mAccount->needAddToNetwork())
	{
		mMainInterface["background"]->addItem(thmb->mThumb);
		ModuleInput* theInputModule = (ModuleInput*)KigsCore::GetModule("ModuleInput");
		theInputModule->getTouchManager()->registerEvent(thmb->mThumb.get(), "ManageClickEvent", Click, EmptyFlag);

		thmb->mThumb->InsertFunction("ManageClickEvent", [thmb](ClickEvent& event) -> bool
			{
				if (static_cast<UIItem*>(event.item)->CanInteract(event.position.xy))
				{
					v2f pos(0.1f + (rand() % 800) * 0.001f, 0.1f + (rand() % 800) * 0.001f);

					thmb->mPos.Set(pos.x * 1920.0f, pos.y * 1080.0f);

					return true;
				}
			});

	}
	else
	{
		mMainInterface["fixedbackground"]->addItem(thmb->mThumb);

		v2f pos(0.5f + 0.45f * cosf(mCurrentOutNetAngle), 0.5f + 0.46f * sinf(mCurrentOutNetAngle));

		thmb->mPos.Set(pos.x * 1920.0f, pos.y * 1080.0f);
		mCurrentOutNetAngle += fPI / 12.0f;
	}

	thmb->mThumb->setValue("Dock", v2f(thmb->mPos.x / 1920.0f, thmb->mPos.y / 1080.0f));
}


std::vector<std::pair<RTNetwork::displayThumb*, std::pair<u32,u32>>> RTNetwork::getValidLinks(u64 uID, bool& foundStrongLink, bool includeEmpty)
{
	foundStrongLink = false;
	std::vector<std::pair<displayThumb*, std::pair<u32, u32>>> links;
	for (auto a : mThumbs)
	{

		if (a->mAccount->mID != uID)
		{
			u32 totalHasCoef = a->mAccount->getHasRTCount();
			u32 totalWasCoef = a->mAccount->getWasRTCount();

			u32 totalHasCoef_pc[2] = { (totalHasCoef * 2) / 100,(totalHasCoef * 4) / 100 };
			u32 totalWasCoef_pc[2] = { (totalWasCoef * 5) / 100,(totalWasCoef * 20) / 100 };

			int d = a->mAccount->getDepth();

			if ((d >= 0) && (d < 2))
			{
				u32 lHascoef = a->mAccount->getHasRTCoef(uID);
				u32 lWascoef = a->mAccount->getWasRTCoef(uID);
				if ((lHascoef > totalHasCoef_pc[d]) ||((lWascoef > totalWasCoef_pc[d])))
				{
					if ((d == 0) && (lHascoef > totalHasCoef_pc[1]))
					{
						foundStrongLink = true;
					}

					links.push_back({ a,{lHascoef,lWascoef} });
				}
				else
				{
					if(includeEmpty)
						links.push_back({ a, {0,0} });
				}
			}
			else
			{
				if (includeEmpty)
					links.push_back({ a,  {0,0} });
			}
		}
	}

	if (links.size())
	{
		std::sort(links.begin(), links.end(), [](const std::pair<displayThumb*, std::pair<u32,u32>>& a1, const std::pair<displayThumb*, std::pair<u32, u32>>& a2)
			{
				if ((a1.second.first + (a1.second.second >> 2)) == (a2.second.first + (a2.second.second >> 2)))
				{
					return a1.first > a2.first;
				}
				return ((a1.second.first+(a1.second.second>>2)) > (a2.second.first + (a2.second.second >> 2)));
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
	bool stronglink = false;
	std::vector<std::pair<displayThumb*,  std::pair<u32,u32>>> links = getValidLinks(nID, stronglink,true);
	
	std::map<std::pair<u64, u64>, std::pair<thumbLink, displayThumb*>>	tmpToAdd;

	bool validAdd=false;

	if (links.size())
	{
	
		for (auto l : links)
		{
			if (!l.first->mAccount->needAddToNetwork())
			{
				continue;
			}

			thumbLink toAdd;
	
			toAdd.mLength = l.second.first+(l.second.second>>2);

			u64 oID = l.first->mAccount->mID;

			std::pair<displayThumb*, displayThumb*> accountPair(account, l.first);

			std::pair<u64, u64> k(nID, oID);
			if (nID > oID)
			{
				k.first = oID;
				accountPair.first = l.first;
				k.second = nID;
				accountPair.second = account;
			}

			toAdd.mThumbs[0] = accountPair.first;
			toAdd.mThumbs[1] = accountPair.second;

			if (toAdd.mLength > 0.0f)
			{
				CMSP displayL = CoreModifiable::Import("Link.xml", false, false, nullptr);
				toAdd.mDisplayedLink = displayL;
				displayL("Size") = v2f(50,50);
				displayL["link1"]("Color") = accountPair.first->mBackColor;
				displayL["link2"]("Color") = accountPair.second->mBackColor;

				mMainInterface["background"]->addItem(toAdd.mDisplayedLink);
				validAdd = true;
			}

			tmpToAdd[k] = { toAdd,l.first };

		}

	}	

	if (validAdd || (mAnalysedAccountList.size()==0))
	{
		for (auto ta : tmpToAdd)
		{
			mLinks[ta.first] = ta.second.first;

			account->mLink.push_back(&mLinks[ta.first]);
			ta.second.second->mLink.push_back(&mLinks[ta.first]);
		}
		
	}
	else
	{
		account->mAccount->setNeedAddToNetwork(false);
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


	auto getIndex = [this](u64 id)->u32
	{
		u32 index = 0;
		for (auto t : mThumbs)
		{
			if (t->mAccount->mID == id)
			{
				return index;
			}
			index++;
		}
		return (u32)-1;
	};

	size_t findex = 0;
	for (auto t : mThumbs)
	{
		if (t->mAccount->needAddToNetwork())
		{
			std::set<thumbLink*> thisThumbLinks;

			// compute forces
			for (auto& l : t->mLink)
			{
				thisThumbLinks.insert(l);
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
					float llen = t->mRadius + other->mRadius + l->mLength;
					forces[findex] += posdiff * (dist - llen) * l->mSpringCoef;
				}
				else // just repel
				{
					dist -= t->mRadius + other->mRadius;
					if (dist < 1.0f)
						dist = 1.0f;

					forces[findex] -= posdiff * 10000.0f / (10.0f + dist * dist);
				}
			}

			// check if must be pushed from a link
			for (auto& alllinks : mLinks)
			{
				if (thisThumbLinks.find(&alllinks.second) == thisThumbLinks.end()) // this link is not linked to me
				{
					if (alllinks.second.mLength > 0.0f) // this is a visible link
					{
						v2f linkvector(alllinks.second.mThumbs[1]->mPos - alllinks.second.mThumbs[0]->mPos);
						float linkvectorNorm = Norm(linkvector);
						if (linkvectorNorm > 0.0f)
							linkvector *= 1.0f / linkvectorNorm;

						v2f thumbvector(t->mPos - alllinks.second.mThumbs[0]->mPos);

						float dotp = Dot(linkvector, thumbvector);

						if ((dotp <= 0.0f) || (dotp >= linkvectorNorm)) // don't overlap vector
							continue;

						v2f projpos = alllinks.second.mThumbs[0]->mPos + linkvector * dotp;
						v2f projtovector = projpos - t->mPos;

						float normprojpos = Norm(projtovector);

						if (normprojpos < t->mRadius * 1.5f)
						{
							if (normprojpos == 0.0f)
							{
								forces[findex] += v2f(((rand() % 100) - 50.0f) / 1000.0f, ((rand() % 100) - 50.0f) / 1000.0f);
							}
							else
							{
								projtovector *= 1.0f / normprojpos;

								v2f localf = projtovector * 10000.0f / (normprojpos * normprojpos);
								forces[findex] -= localf;

								u32 firstIndex = getIndex(alllinks.first.first);
								u32 secondIndex = getIndex(alllinks.first.second);

								forces[firstIndex] += localf * (1.0f - dotp / linkvectorNorm);
								forces[secondIndex] += localf * (dotp / linkvectorNorm);
							}

						}
					}
				}
			}

			// friction
			forces[findex] -= t->mSpeed * 0.2f;
		}

		findex++;
	}

	v2f bary(0.0f,0.0f);
	findex = 0;
	u32 validbary = 0;
	for (auto& t : mThumbs)
	{
		if (t->mAccount->needAddToNetwork())
		{
			t->mAcceleration = forces[findex];
			if (Norm(t->mAcceleration) > 10.0f)
			{
				t->mAcceleration.Normalize();
				t->mAcceleration *= 10.0f;
			}
			t->mSpeed += t->mAcceleration * dt;
			t->mPos += t->mSpeed * dt;
			bary += t->mPos;
			validbary++;
		}
		findex++;
	}

	if (validbary)
	{
		bary /= (float)validbary;
	}
	bary -= {1920.0f / 2.0f, 1080.0f / 2.0f};

	for (auto t : mThumbs)
	{
		if (t->mAccount->needAddToNetwork())
		{
			t->mPos -= bary;
			t->mThumb->setValue("Dock", v2f(t->mPos.x / 1920.0f, t->mPos.y / 1080.0f));
		}
	}

}

void	RTNetwork::updateLinks()
{
	for (auto& l : mLinks)
	{
		if (!l.second.mDisplayedLink.isNil())
		{

			v2f p1 = l.second.mThumbs[0]->mPos;
			v2f p2 = l.second.mThumbs[1]->mPos;

			std::pair<displayThumb*, displayThumb*> accountPair(l.second.mThumbs[0], l.second.mThumbs[1]);

			float hasRTTotal[2] = { accountPair.first->mAccount->getHasRTCount(),accountPair.second->mAccount->getHasRTCount() };
			float wasRTTotal[2] = { accountPair.first->mAccount->getWasRTCount(),accountPair.second->mAccount->getWasRTCount() };
			float hasRT[2] = { accountPair.first->mAccount->getHasRTCoef(accountPair.second->mAccount->mID),accountPair.second->mAccount->getHasRTCoef(accountPair.first->mAccount->mID) };
			float wasRT[2] = { accountPair.first->mAccount->getWasRTCoef(accountPair.second->mAccount->mID),accountPair.second->mAccount->getWasRTCoef(accountPair.first->mAccount->mID) };

			CMSP displayL = l.second.mDisplayedLink;
			v2f dp(p2 - p1);
			float dist = Norm(dp);

			float offsetxStart = l.second.mThumbs[0]->mRadius / dist;
			float offsetxEnd = l.second.mThumbs[1]->mRadius / dist;

			CoreItemSP	poly1 = CoreItemSP::getCoreVector();

			float newLength = 0.0f;
			float newLengthMult = 0.0f;

			if ((hasRT[0] > 0.0f) || (wasRT[1] > 0.0f))
			{
				float offsetStartY = (hasRTTotal[0] > 0.0f) ? (0.495 - sqrtf(0.5f * hasRT[0] / hasRTTotal[0])) : 0.495;
				float offsetEndY = (wasRTTotal[1] > 0.0f) ? (0.495 - sqrtf(0.5f * wasRT[1] / wasRTTotal[1])) : 0.495;

				poly1->set("", v2f(offsetxStart * 0.8, offsetStartY));
				poly1->set("", v2f(1.0f - offsetxEnd * 0.8, offsetEndY));
				poly1->set("", v2f(1.0f - offsetxEnd, 0.5));
				poly1->set("", v2f(offsetxStart, 0.5));

				displayL["link1"]["shape"]("Vertices") = poly1.get();

				newLength = (-(0.495 - offsetStartY) - -(0.495 - offsetEndY)) * 10.0f;
				newLengthMult += 1.0f;
			}
			else
			{
				if(!displayL["link1"].isNil())
					displayL->removeItem(displayL["link1"]);
			}

			poly1 = nullptr;

			poly1 = CoreItemSP::getCoreVector();


			if ((hasRT[1] > 0.0f) || (wasRT[0] > 0.0f))
			{
				float offsetStartY = (hasRTTotal[1] > 0.0f) ? (0.505 + sqrtf(0.5f * hasRT[1] / hasRTTotal[1])) : 0.505;
				float offsetEndY = (wasRTTotal[0] > 0.0f) ? (0.505 + sqrtf(0.5f * wasRT[0] / wasRTTotal[0])) : 0.505;

				poly1->set("", v2f(1.0f - offsetxEnd * 0.8, offsetStartY));
				poly1->set("", v2f(1.0f - offsetxEnd, 0.5));
				poly1->set("", v2f(offsetxStart, 0.5));
				poly1->set("", v2f(offsetxStart * 0.8, offsetEndY));

				displayL["link2"]["shape"]("Vertices") = poly1.get();

				newLength += ((offsetStartY-0.505) + (offsetEndY-0.505)) * 10.0f;
				newLengthMult += 1.0f;
			}
			else
			{
				if (!displayL["link2"].isNil())
					displayL->removeItem(displayL["link2"]);
			}

			if (newLengthMult > 1.1f)
			{
				l.second.mSpringCoef = 0.1f;
			}
			else
			{
				l.second.mSpringCoef = 0.01f;
			}

			l.second.mLength = powf(newLength,newLengthMult)*6.0f;
			if (l.second.mLength > 300.0f)
				l.second.mLength = 300.0f;
			l.second.mLength = 330.0f - l.second.mLength;


			float rotate = atan2f(dp.y, dp.x);

			v2f prevSize =l.second.mDisplayedLink("Size");


			l.second.mDisplayedLink->setValue("Size", v2f(dist, prevSize.y));
			l.second.mDisplayedLink->setValue("RotationAngle", rotate);
			l.second.mDisplayedLink->setValue("Dock", v2f(l.second.mThumbs[0]->mPos.x / 1920.0f, l.second.mThumbs[0]->mPos.y / 1080.0f) );
		}
	}
}


void	RTNetwork::webscrapperNavigationComplete(CoreModifiable* sender)
{
	std::string geturl = sender->getValue<std::string>("Url");

	if (geturl.find("https://t.co") != std::string::npos) // not decoded
	{
		if (mCountDecodeAttempt==0)
		{
			mCountDecodeAttempt++;
			return;
		}
	}

	mCountDecodeAttempt = 0;

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
		if (hostname.find("facebook") != std::string::npos)
		{
			size_t postspos = url.find("posts");
			if (postspos != std::string::npos)
			{
				size_t	facebookpos = url.rfind("www.facebook.com", postspos - 1);
				if (facebookpos != std::string::npos)
				{
					return "FB:" + url.substr(facebookpos + 16, postspos - facebookpos - 16);
				}
			}
			return "";
		}
		if (hostname.find("instagram") != std::string::npos)
		{
			size_t statuspos = url.find("/status");
			if (statuspos != std::string::npos)
			{
				size_t	facebookpos = url.rfind("www.facebook.com", statuspos - 1);
				if (facebookpos != std::string::npos)
				{
					return "FB:" + url.substr(facebookpos + 16, statuspos - facebookpos - 16);
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