#include <windows.h>
#include <inttypes.h>
#include <TwitterAnalyser.h>
#include <FilePathManager.h>
#include <NotificationCenter.h>
#include "HTTPRequestModule.h"
#include "JSonFileParser.h"
#include "TinyImage.h"
#include "JPEGClass.h"
#include "PNGClass.h"
#include "GIFClass.h"
#include "TextureFileManager.h"
#include "UI/UIImage.h"
#include "Histogram.h"
#include "AnonymousModule.h"


//#define LOG_ALL
#ifdef LOG_ALL
SmartPointer<::FileHandle> LOG_File = nullptr;

static	std::string lastLogLine = "";
static  int		duplicateLineCount = 1;


void openLog()
{
	LOG_File = Platform_fopen("Log_All.txt", "wb");
}

void	writelog(std::string logline)
{
	if (LOG_File->mFile)
	{
		if ((lastLogLine == logline) && (logline != ""))
		{
			duplicateLineCount++;
		}
		else
		{
			if (duplicateLineCount > 1)
			{
				lastLogLine += "x" + std::to_string(duplicateLineCount);
				duplicateLineCount = 1;
			}
			if (lastLogLine != "")
			{
				lastLogLine += "\n";
				Platform_fwrite(lastLogLine.c_str(), 1, lastLogLine.length(), LOG_File.get());
			}
		}
		lastLogLine = logline;
	}
}

void closeLog()
{
	writelog(""); // flush last line if needed
	Platform_fclose(LOG_File.get());
}

#endif

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


IMPLEMENT_CLASS_INFO(TwitterAnalyser);

IMPLEMENT_CONSTRUCTOR(TwitterAnalyser)
{
	srand(time(NULL));
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

void	TwitterAnalyser::ReadScripts()
{

	u64 length;
	CoreRawBuffer* rawbuffer = ModuleFileManager::LoadFileAsCharString("WalkerScript.txt", length, 1);
	if (rawbuffer)
	{
		mWalkInitScript = (const char*)rawbuffer->buffer();
		rawbuffer->Destroy();
		rawbuffer = nullptr;
	}

	rawbuffer = ModuleFileManager::LoadFileAsCharString("CallWalkerScript.txt", length, 1);
	if (rawbuffer)
	{
		mCallWalkScript = (const char*)rawbuffer->buffer();
		rawbuffer->Destroy();
		rawbuffer = nullptr;
	}

	/*rawbuffer = ModuleFileManager::LoadFileAsCharString("ScrollScript.txt", length, 1);
	if (rawbuffer)
	{
		mScrollScript = (const char*)rawbuffer->buffer();
		rawbuffer->Destroy();
		rawbuffer = nullptr;
	}*/
}


void	TwitterAnalyser::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

#ifdef LOG_ALL
	openLog();
#endif

	mCurrentTime = time(0);

	SYSTEMTIME	retrieveYear;
	GetSystemTime(&retrieveYear);
	mCurrentYear = retrieveYear.wYear;

	// here don't use files olders than three months.
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;

	// Init App
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary("launchParams.json");

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

	SetMemberFromParam(mUserName, "UserName");
	SetMemberFromParam(mHashTag, "HashTag");
	SetMemberFromParam(mUserPanelSize, "UserPanelSize");
	SetMemberFromParam(mMaxUserCount, "MaxUserCount");
	SetMemberFromParam(mValidUserPercent, "ValidUserPercent");
	SetMemberFromParam(mWantedTotalPanelSize, "WantedTotalPanelSize");
	// likes mode
	SetMemberFromParam(mUseLikes, "UseLikes");
	SetMemberFromParam(mMaxLikersPerTweet, "MaxLikersPerTweet");	


	int oldFileLimitInDays = 3 * 30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");

	mOldFileLimit = 60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays;

	if (mUseLikes)
	{
		SetMemberFromParam(mDetailedLikeStats, "DetailedLikeStats");

		if (mDetailedLikeStats)
		{
			SetMemberFromParam(mDailyAnalysis, "DailyStats");
			if (mDailyAnalysis)
			{
				mOldFileLimit = 60.0 * 60.0 * 12.0;
			}
		}
		// load anonymous module for web scraper
#ifdef _DEBUG
		mWebScraperModule = new AnonymousModule("WebScraperD.dll");
#else
		mWebScraperModule = new AnonymousModule("WebScraper.dll");
#endif
		mWebScraperModule->Init(KigsCore::Instance(), nullptr);

		mWebScraper= KigsCore::GetInstanceOf("theWebScrapper", "WebViewHandler");

		// can't find anonymous module ? don't use likes
		if (!mWebScraper->isSubType("WebViewHandler"))
		{
			mWebScraperModule->Close();
			delete mWebScraperModule;
			mWebScraper = nullptr;
			mUseLikes = false;
			mDetailedLikeStats = false;
		}
		else
		{
			mWebScraper->Init();
			KigsCore::Connect(mWebScraper.get(), "navigationComplete", this, "LaunchScript");
			mWebScraper->setValue("Url", "https://twitter.com/login");
			KigsCore::Connect(mWebScraper.get(), "msgReceived", this, "treatWebScraperMessage");
			AddAutoUpdate(mWebScraper.get());

			ReadScripts();

		}
	}

	SP<FilePathManager>& pathManager = KigsCore::Singleton<FilePathManager>();
	// when a path is given, search the file only with this path
	pathManager->setValue("StrictPath", true);
	pathManager->AddToPath(".", "json");

	CoreCreateModule(HTTPRequestModule, 0);

	// init twitter connection
	mTwitterConnect = KigsCore::GetInstanceOf("TwitterConnect", "HTTPConnect");
	mTwitterConnect->setValue("HostName", "api.twitter.com");
	mTwitterConnect->setValue("Type", "HTTPS");
	mTwitterConnect->setValue("Port", "443");
	mTwitterConnect->Init();

	std::string currentUserProgress = "Cache/";

	// search if current user is in Cached data

	if (mHashTag.length())
	{

		auto& textureManager = KigsCore::Singleton<TextureFileManager>();
		mCurrentUser.mThumb.mTexture = textureManager->GetTexture("keyw.png");
		if (mHashTag[0] == '#')
		{
			mCurrentUser.mName = " " + mHashTag;
		}
		else
		{
			mCurrentUser.mName = mHashTag;
		}
		currentUserProgress += "HashTag/";
		currentUserProgress += getHashtagFilename() + ".json";
		CoreItemSP currentP = LoadJSon(currentUserProgress);
		if (currentP.isNil()) // new hashtag
		{

			JSonFileParser L_JsonParser;
			CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
			CoreItemSP idP = CoreItemSP::getCoreItemOfType<CoreValue<std::string>>();
			idP = mHashTag;
			initP->set("hashtag", idP);
			L_JsonParser.Export((CoreMap<std::string>*)initP.get(), currentUserProgress);

			std::string url = "1.1/search/tweets.json?q=" + mHashTag + "&count=100&include_entities=false&result_type=mixed";
			mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getHashTagsTweets", this);
			mAnswer->AddHeader(mTwitterBear[NextBearer()]);
			mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
			mAnswer->Init();
			myRequestCount++;
			RequestLaunched(5.1);
		}
		else // load current hashtag 
		{
		
			if (mUseLikes) // save tweets ids
			{
				std::string filename = "Cache/HashTag/" + getHashtagFilename() + ".twts";
				LoadTweetsFile(filename);
				CoreItemSP posters=currentP["Posters"];
				for (auto p : posters)
				{
					mTweetsPosters.push_back(p);
				}
			}
			else // save twittos id
			{
				std::string filename = "Cache/HashTag/" + getHashtagFilename() + ".ids";
				mFollowers = LoadIDVectorFile(filename);

				for (unsigned int i = 0; i < mFollowers.size(); i++)
				{
					u64 id = mFollowers[i];
					mTwittosMap[id] = id;
				}
			}

			if (!currentP["next-cursor"].isNil())
			{
				mFollowersNextCursor = currentP["next-cursor"];
			}

			if ((mFollowersNextCursor != "-1") && (mTweetsPosters.size()<100))
			{
				mState = GET_HASHTAG_CONTINUE;
			}
			else
			{
				if (mUseLikes)
				{
					mState = GET_TWEET_LIKES;
				}
				else
				{
					mState = CHECK_INACTIVES;
				}
			}
		}


	}
	else
	{
		currentUserProgress += "UserName/";
		currentUserProgress += mUserName + ".json";
		CoreItemSP currentP = LoadJSon(currentUserProgress);

		if (currentP.isNil()) // new user
		{
#ifdef LOG_ALL
			writelog("new user : request details");
#endif

			// check classic User Cache
			std::string url = "2/users/by/username/" + mUserName + "?user.fields=created_at,public_metrics,profile_image_url";  
			mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
			mAnswer->AddHeader(mTwitterBear[NextBearer()]);
			mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
			mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", true);
			mAnswer->Init();
			myRequestCount++;
			RequestLaunched(1.1);
		}
		else // load current user
		{
			mUserID = currentP["id"];

			LoadUserStruct(mUserID, mCurrentUser, true);

			// look if needs more followers
			std::string filename = "Cache/UserName/";
			filename += mUserName + ".json";
			CoreItemSP currentUserJson = LoadJSon(filename);

			if (!currentUserJson["next-cursor"].isNil())
			{
				mFollowersNextCursor = currentUserJson["next-cursor"];
			}

			// get followers
			mState = GET_FOLLOWERS_INIT;

		}
	}

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
	mLastUpdate = GetApplicationTimer()->GetTime();
}

void	TwitterAnalyser::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();

	if (mDrawForceBased)
	{
		DrawForceBased();
		return;
	}

	if (mWaitQuota)
	{
		double dt = GetApplicationTimer()->GetTime() - mStartWaitQuota;
		// 2 minutes
		if (dt > (2.0 * 60.0))
		{
#ifdef LOG_ALL
			writelog("Wait Quota : relaunch request");
#endif

			mWaitQuota = false;
			mAnswer->GetRef(); 
			KigsCore::addAsyncRequest(mAnswer.get());
			mAnswer->Init();
			RequestLaunched(60.5);
		}
		
	}
	else 
	{
		switch (mState)
		{
		case WAIT_STATE: // wait
#ifdef LOG_ALL
			writelog("WAIT_STATE");
#endif
			break;

		case GET_HASHTAG_CONTINUE:
		{
			if (mFollowersNextCursor != "-1")
			{
				if (CanLaunchRequest())
				{
					std::string url = "1.1/search/tweets.json" + mFollowersNextCursor;
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getHashTagsTweets", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->Init();
					myRequestCount++;
					mState = WAIT_STATE;
					RequestLaunched(5.1);
				}
			}
			else
			{
				if (mUseLikes)
				{
					mState = GET_TWEET_LIKES;
				}
				else
				{
					mState = CHECK_INACTIVES;
				}
			}

		}
			break;
		case GET_FOLLOWERS_INIT: // get follower list
		{
			if ((mFollowers.size() + mTweets.size()) == 0)
			{
				// try to load followers file
				if (mUseLikes)
				{
					if (LoadTweetsFile())
					{
						mState = GET_TWEET_LIKES;
						break;
					}
				}
				else
				{
					if (LoadFollowersFile() && (mFollowersNextCursor == "-1"))
					{
						mState = CHECK_INACTIVES;
						break;
					}
				}
			}
		}
		case GET_FOLLOWERS_CONTINUE:
		{
			mState = GET_FOLLOWERS_CONTINUE;
			// check that we reached the end of followers
			if (((mFollowers.size()+mTweets.size()) == 0) || (mFollowersNextCursor != "-1"))
			{

				if (CanLaunchRequest())
				{
#ifdef LOG_ALL
					writelog("getFollowers for " + mUserName + " request");
#endif
					if (mUseLikes)
					{
						// get user tweets

						//https://api.twitter.com/2/users/:id/tweets?tweet.fields=created_at&expansions=author_id&user.fields=created_at&max_results=5
						// TODO add until_id for next page
						std::string url = "2/users/" + std::to_string(mUserID) + "/tweets?tweet.fields=public_metrics";

						if (mDailyAnalysis)
						{
							std::string yesterday = GetDate(-24 * 60 * 60);
							url += "&start_time=" + yesterday + "00:00:00Z";
							url += "&end_time=" + yesterday + +"23:59:59Z";
						}

						url += "&max_results=100";
						if (mFollowersNextCursor != "-1")
						{
							url += "&pagination_token=" + mFollowersNextCursor;
						}
						mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getTweets", this);
						mAnswer->AddHeader(mTwitterBear[NextBearer()]);
						mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
						mAnswer->Init();
						myRequestCount++;
						mState = WAIT_STATE;
						RequestLaunched(60.5);
					}
					else
					{
						// need to ask more data
						std::string url = "1.1/followers/ids.json?cursor=" + mFollowersNextCursor + "&screen_name=" + mUserName + "&count=5000";
						mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFollowers", this);
						mAnswer->AddHeader(mTwitterBear[NextBearer()]);
						mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
						mAnswer->Init();
						myRequestCount++;
						mState = WAIT_STATE;
						RequestLaunched(60.5);
					}
				}
			}

			break;
		}
		case GET_TWEET_LIKES:
		{
			if (mCurrentTreatedTweetIndex<mTweets.size())
			{
				u64 tweetID = mTweets[mCurrentTreatedTweetIndex].mTweetID;

				std::string username = mUserName;
				if (mHashTag.length())
				{
					username = mTweetsPosters[mCurrentTreatedTweetIndex];
				}

				CoreItemSP likers = LoadLikersFile(tweetID, username);
				if (!likers.isNil())
				{
					mTweetLikers.clear();
					mCurrentTreatedLikerIndex = 0;
					mValidTreatedLikersForThisTweet = 0;
					for (auto l : likers)
					{
						mTweetLikers.push_back(l);
					}

					mState = GET_USER_FAVORITES;
					break;
				}
				else
				{
					std::string stringID = GetIDString(tweetID);

					std::string username = mUserName;
					if (mTweetsPosters.size())
					{
						username = mTweetsPosters[mCurrentTreatedTweetIndex];
					}

					std::string ScrapURL = "https://twitter.com/" + username + "/status/" + stringID + "/likes";
					/*mNextScript = "const walk = (el) => {"\
						"window.chrome.webview.postMessage(JSON.stringify(el,[\"href\",\"id\", \"tagName\", \"className\"]));"\
						"Array.from(el.children).forEach(walk);"\
						"};"\
						"walk(document.querySelector('[aria-label=\"Fil d'actualités : Aimé par\"]'));"\
						"window.chrome.webview.postMessage(\"scriptDone\");";*/
					mNextScript = mWalkInitScript + mCallWalkScript;
					mScraperState = GET_LIKES;
					mWebScraper->setValue("Url", ScrapURL);
					mState = WAIT_STATE;
				}
			}
			else // treat next tweet
			{
				if (mHashTag.length())
				{
					// load next-cursor
					std::string currentUserProgress = "Cache/";
					currentUserProgress += "HashTag/";
					currentUserProgress += getHashtagFilename() + ".json";
					CoreItemSP currentP = LoadJSon(currentUserProgress);
					if (!currentP["next-cursor"].isNil())
					{
						mFollowersNextCursor = currentP["next-cursor"];
					}
					if (mFollowersNextCursor != "-1")
					{
						mState = GET_HASHTAG_CONTINUE;
					}
					else
					{
						mState = EVERYTHING_DONE;
					}
				}
				else
				{
					mState = GET_FOLLOWERS_CONTINUE;
				}
			}
			break;
		}
		case CHECK_INACTIVES: // check fake
		{
			u64 currentFollowerID = mFollowers[mTreatedFollowerIndex];
#ifdef LOG_ALL
			writelog("CHECK_INACTIVES " + std::to_string(currentFollowerID) );
#endif
			UserStruct	tmpuser;
			if (!LoadUserStruct(currentFollowerID, tmpuser, false))
			{
				mUserDetailsAsked.push_back(currentFollowerID);
				mState = GET_USER_DETAILS_FOR_CHECK;	// ask for user detail and come back here
			}
			else // check if follower seems fake
			{
				int deltaYear=1;

				if (tmpuser.UTCTime != "")
				{
					int creationYear = getCreationYear(tmpuser.UTCTime);
					deltaYear = 1 + (mCurrentYear - creationYear);
				}

				if ((tmpuser.mFollowersCount < 4) && (tmpuser.mFollowingCount < 30) && ((tmpuser.mStatuses_count/deltaYear)<3) ) // this is FAKE NEEEWWWWSS !
				{
#ifdef LOG_ALL
					writelog("Is Inactive " + std::to_string(currentFollowerID));
#endif
					mFakeFollowerCount++;
					NextTreatedFollower();
					if ((mTreatedFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
					{
						mState = EVERYTHING_DONE;
						break;
					}
				}
				else
				{
					mState = TREAT_FOLLOWER; // this follower is OK, treat it
				}
			}
		}
		break;
		case GET_USER_FAVORITES:
		{	
			if ((mCurrentTreatedLikerIndex < mTweetLikers.size()) && ((mValidTreatedLikersForThisTweet <mMaxLikersPerTweet)||(!mMaxLikersPerTweet)))
			{
				std::string user = mTweetLikers[mCurrentTreatedLikerIndex];
				auto found = mFoundLiker.find(user);
				if (found != mFoundLiker.end()) // this one was already treated
				{
					(*found).second++;
					mCurrentTreatedLikerIndex++; // goto next one
					break;
				}
			
				if (LoadFavoritesFile(user))
				{
					mState = GET_USER_ID;
					break;
				}
				else
				{
					
					if (CanLaunchRequest())
					{
						std::string url = "1.1/favorites/list.json?screen_name=" + user + "&count=200&include_entities=false";
						mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFavorites", this);
						mAnswer->AddHeader(mTwitterBear[NextBearer()]);
						mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
						mAnswer->Init();
						myRequestCount++;
						mState = WAIT_STATE;
						RequestLaunched(20.5);
					}
				}
			}
			else // treat next tweet
			{
				mCurrentTreatedTweetIndex++;
				mTweetLikers.clear();
				mCurrentTreatedLikerIndex = 0;
				mValidTreatedLikersForThisTweet = 0;
				mState = GET_TWEET_LIKES;
			}
			break;
		}
		case GET_LIKER_FOLLOWING:
		case TREAT_FOLLOWER: // treat one follower
		{

			// search for an available next-cursor for current following
			u64 currentFollowerID;
			if (mUseLikes)
			{
				currentFollowerID = mCurrentLikerID;
			}
			else
			{
				currentFollowerID = mFollowers[mTreatedFollowerIndex];
			}
#ifdef LOG_ALL
			writelog("TREAT_FOLLOWER " + std::to_string(currentFollowerID));
#endif
			std::string stringID = GetIDString(currentFollowerID);
			std::string filename = "Cache/Users/" + GetUserFolderFromID(currentFollowerID) + "/" + stringID + "_nc.json";
			CoreItemSP currentNextCursor = LoadJSon(filename);
			bool needRequest = false;
			std::string nextCursor = "-1";
			if (currentNextCursor.isNil())
			{
				needRequest = true;
			}
			else
			{
				nextCursor = currentNextCursor["next-cursor"];
				if (nextCursor != "-1")
				{
					needRequest = true;
				}
			}
			
			if (!needRequest)
			{
				LoadFollowingFile(currentFollowerID);
				if (mUseLikes)
				{
					mState = UPDATE_LIKES_STATISTICS;
				}
				else
				{
					mState = UPDATE_STATISTICS;
				}
				break;
			}
			else
			{
				if (CanLaunchRequest())
				{
					std::string url = "1.1/friends/ids.json?cursor=" + nextCursor + "&user_id=" + stringID + "&count=5000";
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFollowing", this);
					mAnswer->AddDynamicAttribute<maULong, u64>("UserID", currentFollowerID);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->Init();
					myRequestCount++;
					mState = WAIT_STATE;
					RequestLaunched(60.5);
				}
			}

			break;
		}

		case UPDATE_LIKES_STATISTICS:
		{
			std::string user = mTweetLikers[mCurrentTreatedLikerIndex];
			if (mFavorites[user].size())
			{
				UpdateLikesStatistics();
				mValidFollowerCount++;
				mValidTreatedLikersForThisTweet++;
			}
			
			mFoundLiker[user] = 1;
			
			mState = GET_USER_FAVORITES;
			mCurrentTreatedLikerIndex++;
		
			if (mValidFollowerCount == mUserPanelSize)
			{
				mState = EVERYTHING_DONE;
				break;
			}
			if (mUserDetailsAsked.size()) // can only go there when validFollower is true
			{
				mBackupState = GET_USER_FAVORITES;
				mState = GET_USER_DETAILS;
				break;
			}
		}
		break;
		case UPDATE_STATISTICS: // update statistics
		{
			// if only one followning, just skip ( probably 
			if (mCurrentFollowing.size() > 1)
			{
				u64 currentFollowerID = mFollowers[mTreatedFollowerIndex];
#ifdef LOG_ALL
				writelog("UpdateStatistics " + std::to_string(currentFollowerID));
#endif
				mCheckedFollowerList[currentFollowerID]= mCurrentFollowing;
				mValidFollowerCount++;

				UpdateStatistics();
			}
			else
			{
				printf("closed ?");
			}
			mCurrentFollowing.clear();

			if (mUserDetailsAsked.size()) // can only go there when validFollower is true
			{
				mState = GET_USER_DETAILS;
				break;
			}
#ifdef LOG_ALL
			writelog("NextTreatedFollower ");
#endif
			NextTreatedFollower();
			if ( (mTreatedFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
			{
				mState = EVERYTHING_DONE;
				break;
			}

			mState = CHECK_INACTIVES;

			break;
		}

		case WAIT_USER_ID:
			// just wait
			break;

		case GET_USER_ID:
		{
			std::string user = mTweetLikers[mCurrentTreatedLikerIndex];
			if (mFavorites[user].size())
			{
				std::string filename = "Cache/Tweets/" + user.substr(0, 4) + "/" + user + ".json";
				CoreItemSP currentUserJson = LoadJSon(filename);
				if (currentUserJson.isNil())
				{
					if (CanLaunchRequest())
					{
						std::string url = "2/users/by/username/" + user + "?user.fields=created_at,public_metrics,profile_image_url";
						mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
						mAnswer->AddHeader(mTwitterBear[NextBearer()]);
						mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
						mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", false);
						mAnswer->Init();
						myRequestCount++;
						mState = WAIT_USER_ID;
						RequestLaunched(1.1);
					}
				}
				else
				{
					u64 userID = currentUserJson["id"];
					UserStruct	tmpuser;
					if (!LoadUserStruct(userID, tmpuser, false))
					{
						mUserDetailsAsked.push_back(userID);
						mState = GET_USER_DETAILS_FOR_CHECK;	// ask for user detail and come back here
						mBackupState = GET_USER_ID;
					}
					else
					{
						if (mDetailedLikeStats)
						{
							mCurrentLikerID = userID;
							mState = GET_LIKER_FOLLOWING;
						}
						else
						{
							mState = UPDATE_LIKES_STATISTICS;
						}
					}
				}
			}
			else
			{
				mState = UPDATE_LIKES_STATISTICS;
			}
		}
		break;
		case WAIT_USER_DETAILS_FOR_CHECK: // wait for 
		case WAIT_USER_DETAILS:
#ifdef LOG_ALL
			writelog("WAIT_USER_DETAILS_FOR_CHECK ");
#endif
			break;
		case GET_USER_DETAILS_FOR_CHECK:
#ifdef LOG_ALL
			writelog("GET_USER_DETAILS_FOR_CHECK ");
#endif
		case GET_USER_DETAILS: // update users details
		{
#ifdef LOG_ALL
			writelog("GET_USER_DETAILS ");
#endif
			if (mUserDetailsAsked.size())
			{
				if (CanLaunchRequest())
				{
					std::string url = "2/users/" + std::to_string(mUserDetailsAsked.back()) + "?user.fields=created_at,public_metrics,profile_image_url";
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
					mAnswer->AddDynamicAttribute<maULong, u64>("UserID", mUserDetailsAsked.back());
					mAnswer->Init();
					myRequestCount++;
					mUserDetailsAsked.pop_back();
					RequestLaunched(1.1);
					if ((mState == GET_USER_DETAILS_FOR_CHECK) || mUseLikes) // if state is 
						mState = WAIT_USER_DETAILS_FOR_CHECK;
					else
						mState = WAIT_USER_DETAILS;
				}
			}
			else
			{
				if (mUseLikes)
				{
					if (mBackupState != WAIT_STATE)
					{
						mState = mBackupState;
						mBackupState = WAIT_STATE;
					}
					else
					{
						mState = GET_USER_FAVORITES;
					}
				}
				else
				{

					if (mState == GET_USER_DETAILS) // update statistics was already done, just go to next
					{
#ifdef LOG_ALL
						writelog("NextTreatedFollower ");
#endif
						NextTreatedFollower();
						if ((mTreatedFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
						{
							mState = EVERYTHING_DONE;
							break;
						}
					}
					mState = CHECK_INACTIVES;
				}
			}

			break;
		}
		case EVERYTHING_DONE:
		{
			// done
			if ((!mDetailedStatsWereExported) && (mDetailedLikeStats))
			{
				ExportDetailedStats();

				mDetailedStatsWereExported = true;
			}


			break;
		}
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
			if (mUseLikes)
			{
				sprintf(textBuffer, "Treated Likers : %d", mValidFollowerCount);
			}
			else
			{
				sprintf(textBuffer, "Treated Users : %d", mValidFollowerCount);
			}
			mMainInterface["TreatedFollowers"]("Text") = textBuffer;
			if (mUseLikes)
			{
				sprintf(textBuffer, "Liked user count : %d", mFollowersFollowingCount.size());
			}
			else
			{
				sprintf(textBuffer, "Found followings : %d", mFollowersFollowingCount.size());
			}
			mMainInterface["FoundFollowings"]("Text") = textBuffer;
			if (mUseLikes)
			{
				sprintf(textBuffer, "Invalid likers count : %d", mFakeFollowerCount);
			}
			else
			{
				sprintf(textBuffer, "Inactive Followers : %d", mFakeFollowerCount);
			}
			mMainInterface["FakeFollowers"]("Text") = textBuffer;

			if (mState != EVERYTHING_DONE)
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
				if (mUseLikes && mDetailedLikeStats)
				{
					sprintf(textBuffer, "Liker & follower : %d",(int)(100.0f*(float)mLikerFollowerCount/(float)mValidFollowerCount));

					std::string percentText = textBuffer;
					percentText += "%";

					mMainInterface["RequestCount"]("Text") = percentText;
				}
				else
				{
					mMainInterface["RequestCount"]("Text") = "";
				}
				mMainInterface["RequestWait"]("Text") = "";
				mMainInterface["switchForce"]("IsHidden") = false;
				mMainInterface["switchForce"]("IsTouchable") = true;
				//mMainInterface["FakeFollowers"]("Text") = "";
			}

			// check if Channel texture was loaded

			if (mCurrentUser.mThumb.mTexture && mMainInterface["thumbnail"])
			{
				const SP<UIImage>& tmp = mMainInterface["thumbnail"];

				if (!tmp->HasTexture())
				{
					tmp->addItem(mCurrentUser.mThumb.mTexture);
					mMainInterface["thumbnail"]["UserName"]("Text") = mCurrentUser.mName;
				}
			}
			else if (mMainInterface["thumbnail"])
			{
				LoadUserStruct(mUserID, mCurrentUser, true);
			}

			refreshAllThumbs();
		}
	}
}

void	TwitterAnalyser::ProtectedClose()
{
#ifdef LOG_ALL
	closeLog();
#endif

	DataDrivenBaseApplication::ProtectedClose();
	mTwitterConnect = nullptr;
	mAnswer = nullptr;
}

void	TwitterAnalyser::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;
		if(mUseLikes)
			mMainInterface["heart"]("IsHidden") = false;

	}
}
void	TwitterAnalyser::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = nullptr;

		// delete all channel structs
		mFollowersFollowingCount.clear();
		mShowedUser.clear();
		mCurrentUser.mThumb.mTexture = nullptr;
		mDownloaderList.clear();
		mAccountSubscriberMap.clear();

	}
}

CoreItemSP	TwitterAnalyser::RetrieveJSON(CoreModifiable* sender)
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

void		TwitterAnalyser::SaveJSon(const std::string& fname,const CoreItemSP& json, bool utf16)
{
#ifdef LOG_ALL
	writelog("SaveJSon " + fname);
#endif

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

bool TwitterAnalyser::checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle,double OldFileLimit)
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

CoreItemSP	TwitterAnalyser::LoadJSon(const std::string& fname, bool utf16)
{
	SmartPointer<::FileHandle> filenamehandle;

	CoreItemSP initP(nullptr);
	if (checkValidFile(fname, filenamehandle, mOldFileLimit))
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



DEFINE_METHOD(TwitterAnalyser, getFollowers)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		CoreItemSP followersArray = json["ids"];
		unsigned int idcount=followersArray->size();
		for (unsigned int i = 0; i < idcount; i++)
		{
			u64 id = followersArray[i];
			mFollowers.push_back(id);
		}

		std::string nextStr = json["next_cursor_str"];
		if (nextStr == "0")
		{
			nextStr = "-1";
		}

		// do it again
		if ((mFollowers.size() < mWantedTotalPanelSize)&&(nextStr!="-1"))
		{
			mState = GET_FOLLOWERS_CONTINUE;
			mFollowersNextCursor = nextStr;
		}
		else
		{
			mState = CHECK_INACTIVES;
			mFollowersNextCursor = "-1";
		}
		SaveFollowersFile();
		std::string filename = "Cache/UserName/";
		filename += mUserName + ".json";
		CoreItemSP currentUserJson = LoadJSon(filename);
		currentUserJson->set("next-cursor", mFollowersNextCursor);
		SaveJSon(filename, currentUserJson);
	}

	return true;
}


DEFINE_METHOD(TwitterAnalyser, getFavorites)
{
	auto json = RetrieveJSON(sender);

	std::string user = mTweetLikers[mCurrentTreatedLikerIndex];

	if (!json.isNil())
	{
		std::vector<favoriteStruct>& currentFavorites = mFavorites[user];

		for (const auto& fav : json)
		{
			u64		tweetID=fav["id"];
			u64		userid= fav["user"]["id"];
			u32		likes_count = fav["favorite_count"];
			u32		rt_count = fav["retweet_count"];

			currentFavorites.push_back({ tweetID ,userid ,likes_count ,rt_count });
		}
		

	}
	
	if (!mWaitQuota) // can't access favorite for this user
	{
		std::string user = mTweetLikers[mCurrentTreatedLikerIndex];

		SaveFavoritesFile(user);
		if (mDetailedLikeStats)
			mState = GET_USER_ID;
		else
			mState = UPDATE_LIKES_STATISTICS;
	}

	return true;
}


DEFINE_METHOD(TwitterAnalyser, getTweets)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		std::vector<Twts> retrievedTweets;
		CoreItemSP tweetsArray = json["data"];
		unsigned int tweetcount = tweetsArray->size();
		for (unsigned int i = 0; i < tweetcount; i++)
		{
			u64 tweetid = tweetsArray[i]["id"];
			u32 like_count= tweetsArray[i]["public_metrics"]["like_count"];
			u32 rt_count = tweetsArray[i]["public_metrics"]["retweet_count"];
			if (like_count>1)
			{
				retrievedTweets.push_back({ tweetid,like_count,rt_count });
			}
		}

		if (retrievedTweets.size())
		{
			randomizeVector(retrievedTweets);
			mTweets.insert(mTweets.end(), retrievedTweets.begin(), retrievedTweets.end());
		}
		std::string nextStr = "-1";

		CoreItemSP meta = json["meta"];
		if (!meta.isNil())
		{
			if (!meta["next_token"].isNil())
			{
				nextStr = meta["next_token"];
				if (nextStr == "0")
				{
					nextStr = "-1";
				}
			}
		}

		// if some valid tweets were retrieve, then treat them
		// process will come back here after tweets were treated
		if (mTweets.size())
		{
			mState = GET_TWEET_LIKES;
		}
	
		// use mFollowersNextCursor to store tweets next cursor
		mFollowersNextCursor = nextStr;

		SaveTweetsFile();
		std::string filename = "Cache/UserName/";
		filename += mUserName + ".json";
		CoreItemSP currentUserJson = LoadJSon(filename);
		currentUserJson->set("next-cursor", mFollowersNextCursor);
		SaveJSon(filename, currentUserJson);
	}

	return true;
}



DEFINE_METHOD(TwitterAnalyser, getFollowing)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{

		u64 currentID=sender->getValue<u64>("UserID");
#ifdef LOG_ALL
		writelog("getFollowing request " + std::to_string(currentID) + "ok");
#endif
		CoreItemSP followingArray = json["ids"];
		unsigned int idcount = followingArray->size();
		for (unsigned int i = 0; i < idcount; i++)
		{
			u64 id = followingArray[i];
			mCurrentFollowing.push_back(id);
		}

		SaveFollowingFile(currentID);

		std::string stringID = GetIDString(currentID);
		std::string filename = "Cache/Users/" + GetUserFolderFromID(currentID) + "/" + stringID + "_nc.json";

		CoreItemSP currentP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
		std::string nextStr = json["next_cursor_str"];

		// limit following count to last 20000
		if (mCurrentFollowing.size() >= 20000)
		{
			nextStr = "-1";
		}

		if (nextStr == "0")
		{
			nextStr = "-1";
		}
		currentP->set("next-cursor", nextStr);

		SaveJSon(filename, currentP);

		mState = TREAT_FOLLOWER;
	}
	else
	{
		if (!mWaitQuota)
		{
			// this user is not available, go to next one
			u64 currentID = sender->getValue<u64>("UserID");
#ifdef LOG_ALL
			writelog("getFollowing request " + std::to_string(currentID) + "failed");
#endif
			SaveFollowingFile(currentID);

			std::string stringID = GetIDString(currentID);
			std::string filename = "Cache/Users/" + GetUserFolderFromID(currentID) + "/" + stringID + "_nc.json";

			CoreItemSP currentP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();

			std::string nextStr = "-1";
			currentP->set("next-cursor", nextStr);

			SaveJSon(filename, currentP);

			if (mUseLikes)
			{
				mState = UPDATE_LIKES_STATISTICS;
			}
			else
			{
				mState = UPDATE_STATISTICS;
			}
		}
	}

	return true;
}

DEFINE_METHOD(TwitterAnalyser, getHashTagsTweets)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		CoreItemSP tweetsArray = json["statuses"];
		unsigned int idcount = tweetsArray->size();

		if (mUseLikes) // save tweets ids
		{
			for (unsigned int i = 0; i < idcount; i++)
			{
				CoreItemSP currentTweet = tweetsArray[i];
				
				u32 like_count = currentTweet["favorite_count"];
				if (like_count > 1)
				{
					u32 rt_count = currentTweet["retweet_count"];
					u64 tweetid = currentTweet["id"];

					mTweets.push_back({ tweetid,like_count,rt_count });
					mTweetsPosters.push_back(currentTweet["user"]["screen_name"]);
				}
			}

			std::string filename = "Cache/HashTag/" + getHashtagFilename() + ".twts";
			SaveTweetsFile(filename);		
		}
		else // save twittos id
		{
			for (unsigned int i = 0; i < idcount; i++)
			{
				u64 id = tweetsArray[i]["user"]["id"];
				if (mTwittosMap.find(id) == mTwittosMap.end())
				{
					mTwittosMap[id] = id;
					mFollowers.push_back(id);
				}
			}

			std::string filename = "Cache/HashTag/" + getHashtagFilename() + ".ids";

			SaveIDVectorFile(mFollowers, filename);

		}

		std::string nextStr = "-1";

		if(!json["search_metadata"]["next_results"].isNil())
			nextStr=json["search_metadata"]["next_results"];

		if (nextStr == "0")
		{
			nextStr = "-1";
		}

		if (mUseLikes && (mTweets.size() < (mCurrentTreatedTweetIndex+100)))
		{
			mFollowersNextCursor = nextStr;
		}
		else if(!mUseLikes && (mFollowers.size() < mWantedTotalPanelSize))
		{
			mFollowersNextCursor = nextStr;
		}
		else
		{
			mFollowersNextCursor = "-1";
		}
		mState = GET_HASHTAG_CONTINUE;
		std::string filename = "Cache/HashTag/";
		filename += getHashtagFilename() + ".json";
		CoreItemSP currentUserJson = LoadJSon(filename);
		currentUserJson->set("next-cursor", nextStr);

		if (mTweetsPosters.size())
		{
			CoreItemSP posters = CoreItemSP::getCoreVector();
			currentUserJson->set("Posters", posters);
			for (const auto& p : mTweetsPosters)
			{
				posters->set("", p);
			}
		}

		SaveJSon(filename, currentUserJson);
	}
	return true;
}


DEFINE_METHOD(TwitterAnalyser, getUserDetails)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		CoreItemSP	data = json["data"];
		CoreItemSP	public_m = data["public_metrics"];

		u64 currentID= data["id"];

#ifdef LOG_ALL
		writelog("getUserDetails " + std::to_string(currentID) + " ok");
#endif

		UserStruct	tmp;
		UserStruct* pUser = &tmp;


		if (mState == WAIT_USER_ID)
		{
			JSonFileParser L_JsonParser;
			CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
			CoreItemSP idP = CoreItemSP::getCoreItemOfType<CoreValue<u64>>();
			idP = currentID;
			initP->set("id", idP);
			std::string user= data["username"];
			std::string filename = "Cache/Tweets/" + user.substr(0, 4) + "/" + user + ".json";
			L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
		}
		else if (data["username"] == mUserName)
		{
			mUserID = currentID;
			pUser = &mCurrentUser;
		}
		else 
		{
			pUser = &mFollowersFollowingCount[currentID].second;
		}

		pUser->mName = data["username"];
		pUser->mFollowersCount = public_m["followers_count"];
		pUser->mFollowingCount = public_m["following_count"];
		pUser->mStatuses_count = public_m["tweet_count"];
		pUser->UTCTime = data["created_at"];
		pUser->mThumb.mURL = CleanURL(data["profile_image_url"]);

		SaveUserStruct(currentID, *pUser);
		bool requestThumb;
		if (sender->getValue("RequestThumb", requestThumb))
		{
			if(requestThumb)
			if (!LoadThumbnail(currentID, *pUser))
			{
				LaunchDownloader(currentID, *pUser);
			}
		}
		if (mState == WAIT_USER_ID)
		{
			mState = GET_USER_ID;
		}
		else if (data["username"] == mUserName)
		{
			// save user
			JSonFileParser L_JsonParser;
			CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
			CoreItemSP idP = CoreItemSP::getCoreItemOfType<CoreValue<u64>>();
			idP = mUserID;
			initP->set("id", idP);
			std::string filename = "Cache/UserName/";
			filename += mUserName + ".json";
			L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);

			// get followers
			mState = GET_FOLLOWERS_INIT;
		}

		if (mState == WAIT_USER_DETAILS_FOR_CHECK)
		{
			mState = GET_USER_DETAILS_FOR_CHECK;
		}
		else if (mState == WAIT_USER_DETAILS)
		{
			mState = GET_USER_DETAILS;
		}
		
	}
	else if(!mWaitQuota)
	{

		if (mApiErrorCode == 63) // user suspended
		{
			u64 id=sender->getValue<u64>("UserID");
			UserStruct suspended;
			suspended.mFollowersCount = 0;
			suspended.mFollowingCount = 0;
			suspended.mStatuses_count = 0;
			SaveUserStruct(id, suspended);
			mApiErrorCode = 0;
			mFakeFollowerCount++;
		}


#ifdef LOG_ALL
		writelog("getUserDetails failed");
#endif
		NextTreatedFollower();
#ifdef LOG_ALL
		writelog("NextTreatedFollower");
#endif
		if ((mTreatedFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
		{
			mState = EVERYTHING_DONE;
		}
		else if (mState == WAIT_USER_DETAILS) // ask user details but user was suspended
		{
			mState = CHECK_INACTIVES;
		}
		else if(mState== WAIT_USER_DETAILS_FOR_CHECK)
		{
			mState = GET_USER_DETAILS_FOR_CHECK;
		}
		else if (mState == WAIT_USER_ID)
		{
			mFavorites.clear();
			mState = GET_USER_ID;
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

void		TwitterAnalyser::LaunchDownloader(u64 id, UserStruct& ch)
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

std::string  TwitterAnalyser::CleanURL(const std::string& url)
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

void	TwitterAnalyser::thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader)
{

	// search used downloader in vector
	std::vector<std::pair<CMSP, std::pair<u64, UserStruct*>> > tmpList;

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
				UserStruct* toFill = p.second.second;

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

void	TwitterAnalyser::switchDisplay()
{
	mMainInterface["switchForce"]("IsHidden") = true;
	mMainInterface["switchForce"]("IsTouchable") = false;

	mCurrentMeasure = (Measure)((mCurrentMeasure + 1) % MEASURE_COUNT);
	if (mHashTag.length() && (mCurrentMeasure == Similarity)) // no similarity with hashtags
	{
		mCurrentMeasure = (Measure)((mCurrentMeasure + 1) % MEASURE_COUNT);
	}
}

std::string	TwitterAnalyser::GetUserFolderFromID(u64 id)
{
	std::string result;

	for (int i = 0; i < 4; i++)
	{
		result +='0'+(id % 10);
		id = id / 10;
	}

	return result;
}

std::string	TwitterAnalyser::GetIDString(u64 id)
{
	char	idstr[64];
	sprintf(idstr,"%llu", id);

	return idstr;
}

// load json channel file dans fill ChannelStruct
bool		TwitterAnalyser::LoadUserStruct(u64 id, UserStruct& ch, bool requestThumb)
{
	std::string filename = "Cache/Users/";

	std::string folderName = GetUserFolderFromID(id);

	filename += folderName + "/" + GetIDString(id) + ".json";

	CoreItemSP initP = LoadJSon(filename, true);

	if (initP.isNil()) // file not found, return
	{
		return false;
	}
#ifdef LOG_ALL
	writelog("loaded user" + std::to_string(id) + "details");
#endif
	ch.mName = initP["Name"];
	ch.mFollowersCount = initP["FollowersCount"];
	ch.mFollowingCount = initP["FollowingCount"];
	ch.mStatuses_count = initP["StatusesCount"];
	ch.UTCTime = initP["CreatedAt"];
	if (!initP["ImgURL"].isNil())
	{
		ch.mThumb.mURL = initP["ImgURL"];
	}
	if (requestThumb && !ch.mThumb.mTexture)
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

void		TwitterAnalyser::SaveUserStruct(u64 id, UserStruct& ch)
{
#ifdef LOG_ALL
	writelog("save user" + std::to_string(id) + "details");
#endif

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

bool		TwitterAnalyser::LoadThumbnail(u64 id, UserStruct& ch)
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

void TwitterAnalyser::ExportDetailedStats()
{
	std::string filename = mUserName;
	if(mDailyAnalysis)
		filename += "_"+GetDate(-24 * 60 * 60);
	else
		filename += "_"+GetDate(0);

	filename += "_likes_detailed_stats.csv";
	float wantedpercent = mValidUserPercent;


	// get all parsed channels and get the ones with more than mValidChannelPercent subscribed users
	std::vector<std::pair<unsigned int, u64>>	toSave;
	for (auto c : mFollowersFollowingCount)
	{
		if (c.first != mUserID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mValidFollowerCount;
				if (percent > wantedpercent)
				{
					toSave.push_back({ c.second.first,c.first });
				}
			}
		}
	}

	std::sort(toSave.begin(), toSave.end(), [](const std::pair<unsigned int, u64>& a1, const std::pair<unsigned int, u64>& a2)
		{
			if (a1.first == a2.first)
			{
				return a1.second > a2.second;
			}
			return (a1.first > a2.first);
		}
	);


	SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
	if (L_File->mFile)
	{
		// save general stats
		char saveBuffer[4096];
		sprintf(saveBuffer, "Liker count,%d\n", mValidFollowerCount);
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "favorite count,%d\n", mFollowersFollowingCount.size());
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "follower percent,%f\n", (100.0f * (float)mLikerFollowerCount / (float)mValidFollowerCount));
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());

		sprintf(saveBuffer, "Like count,%d\n", mFollowersFollowingCount[mUserID].second.mLikesCount);
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());

		std::string header = "Twitter account,liker count,follower count," + mUserName + " follower count,follow both count,likes count\n";

		Platform_fwrite(header.c_str(), 1, header.length(), L_File.get());

		for (const auto& p : toSave)
		{
			auto& current = mFollowersFollowingCount[p.second];

			sprintf(saveBuffer, "\"%s\",%d,%d,%d,%d,%d\n", current.second.mName.ToString().c_str(),p.first, current.second.mLikerFollowerCount, current.second.mLikerMainUserFollowerCount, current.second.mLikerBothFollowCount, current.second.mLikesCount);
			Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		}

		Platform_fclose(L_File.get());
	}

}

void	TwitterAnalyser::refreshAllThumbs()
{
	if (mValidFollowerCount < 10)
		return;

	bool somethingChanged = false;

	float wantedpercent = mValidUserPercent;
	

	// get all parsed account and get the ones with more than mValidChannelPercent subscribed users
	std::vector<std::pair<unsigned int,u64>>	toShow;
	for (auto c : mFollowersFollowingCount)
	{
		if (c.first != mUserID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mValidFollowerCount;
				if (percent > wantedpercent)
				{
					toShow.push_back({ c.second.first,c.first });
				}
			}
		}
	}

	if (toShow.size() == 0)
	{
		return;
	}

	float mainW = 1.0;
	if (mUseLikes)
	{
		mainW = mFollowersFollowingCount[mUserID].second.mW;
	}


	if (mCurrentMeasure == Similarity) // Jaccard or similarity measure on likes
	{
		if (mUseLikes)
		{
			std::sort(toShow.begin(), toShow.end(), [this, mainW](const std::pair<unsigned int, u64>& a1, const std::pair<unsigned int, u64>& a2)
				{
					auto& a1User = mFollowersFollowingCount[a1.second];
					auto& a2User = mFollowersFollowingCount[a2.second];

					float a1Score = a1User.second.mW;
					float a2Score = a2User.second.mW;

					if (a1Score == a2Score)
					{
						return a1.second > a2.second;
					}
					return (a1Score > a2Score);
				}
			);
		}
		else
		{
			std::sort(toShow.begin(), toShow.end(), [this](const std::pair<unsigned int, u64>& a1, const std::pair<unsigned int, u64>& a2)
				{

					auto& a1User = mFollowersFollowingCount[a1.second];
					auto& a2User = mFollowersFollowingCount[a2.second];

					// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
					// a1 subscribers %
					float A1_percent = ((float)a1.first / (float)mValidFollowerCount);
					// a1 intersection size with analysed channel
					float A1_a_inter_b = (float)mCurrentUser.mFollowersCount * A1_percent;
					// a1 union size with analysed channel 
					float A1_a_union_b = (float)mCurrentUser.mFollowersCount + (float)a1User.second.mFollowersCount - A1_a_inter_b;

					// a2 subscribers %
					float A2_percent = ((float)a2.first / (float)mValidFollowerCount);
					// a2 intersection size with analysed channel
					float A2_a_inter_b = (float)mCurrentUser.mFollowersCount * A2_percent;
					// a2 union size with analysed channel 
					float A2_a_union_b = (float)mCurrentUser.mFollowersCount + (float)a2User.second.mFollowersCount - A2_a_inter_b;

					float k1 = A1_a_inter_b / A1_a_union_b;
					float k2 = A2_a_inter_b / A2_a_union_b;

					if (k1 == k2)
					{
						return a1.second > a2.second;
					}
					return (k1 > k2);
				}
			);
		}
	}
	else
	{

		std::sort(toShow.begin(), toShow.end(), [&](const std::pair<unsigned int, u64>& a1, const std::pair<unsigned int, u64>& a2)
			{
				if (mCurrentMeasure != Normalized)
				{
					if (a1.first == a2.first)
					{
						return a1.second > a2.second;
					}
					return (a1.first > a2.first);
				}

				auto& a1User = mFollowersFollowingCount[a1.second];
				auto& a2User = mFollowersFollowingCount[a2.second];

				float a1fcount = (a1User.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)a1User.second.mFollowersCount);
				float a2fcount = (a2User.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)a2User.second.mFollowersCount);



				float A1_w = ((float)a1.first / a1fcount);
				float A2_w = ((float)a2.first / a2fcount);
				if (A1_w == A2_w)
				{
					return a1.second > a2.second;
				}
				return (A1_w > A2_w);
			}
		);
	}
	
	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[tos.second] = 1;
		toShowCount++;

		const auto& a1User = mFollowersFollowingCount[tos.second];	

		if (toShowCount >= mMaxUserCount)
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
			somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			somethingChanged = true;
		}
	}

	float dangle = 2.0f * KFLOAT_CONST_PI / 7.0f;
	float angle = 0.0f;
	float ray = 0.15f;
	float dray = 0.0117f;
	toShowCount = 0;
	
	float NormalizeFollowersCountForShown = 1.0f;

	for (const auto& toS : toShow)
	{
		if (toS.second == mUserID)
		{
			continue;
		}

		auto& toPlace = mFollowersFollowingCount[toS.second];
		float toplacefcount = (toPlace.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)toPlace.second.mFollowersCount);
		NormalizeFollowersCountForShown = toplacefcount;
		break;
	}
	
	for (const auto& toS : toShow)
	{
		if (toS.second == mUserID)
		{
			continue;
		}

		auto& toPlace = mFollowersFollowingCount[toS.second];

		auto found = mShowedUser.find(toS.second);
		if (found != mShowedUser.end())
		{
			const CMSP& toSetup = (*found).second;
			v2f dock(0.53f + ray * cosf(angle), 0.47f + ray * 1.02f * sinf(angle));
			toSetup("Dock") = dock;
			angle += dangle;
			dangle = 2.0f * KFLOAT_CONST_PI / (2.0f + 50.0f * ray);
			ray += dray;
			dray *= 0.98f;
			if (toPlace.second.mName.length())
			{
				toSetup["ChannelName"]("Text") = toPlace.second.mName;
			}
			
			float prescale = 1.0f;
			if (mCurrentMeasure == Similarity) // Jaccard or similarity measure on likes
			{
				if (mUseLikes)
				{
					float coef = toPlace.second.mW / mainW;
					float k = 100.0f* coef;
					toSetup["ChannelPercent"]("Text") = std::to_string((int)(k)) + "ls";
					prescale = 1.5f * k / 100.0f;

					if (prescale < 0.1f)
					{
						prescale = 0.1f;
					}
				}
				else
				{
					// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
					// A subscribers %
					float A_percent = ((float)toPlace.first / (float)mValidFollowerCount);
					// A intersection size with analysed channel
					float A_a_inter_b = (float)mCurrentUser.mFollowersCount * A_percent;
					// A union size with analysed channel 
					float A_a_union_b = (float)mCurrentUser.mFollowersCount + (float)toPlace.second.mFollowersCount - A_a_inter_b;

					float k = 100.0f * A_a_inter_b / A_a_union_b;
					if (k > 100.0f) // avoid Jaccard index greater than 100
					{
						k = 100.0f;
					}
					toSetup["ChannelPercent"]("Text") = std::to_string((int)(k)) + mUnity[mCurrentMeasure];

					prescale = 1.5f * k / 100.0f;

					if (prescale < 0.1f)
					{
						prescale = 0.1f;
					}
				}
			}
			else
			{
				int percent = (int)(100.0f * ((float)toPlace.first / (float)mValidFollowerCount));
				
				if (mCurrentMeasure == Normalized)
				{
					float fpercent = (float)toPlace.first / (float)mValidFollowerCount;
					float toplacefcount = (toPlace.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)toPlace.second.mFollowersCount);
					fpercent *= NormalizeFollowersCountForShown / toplacefcount;
					
					percent = (int)(100.0f * fpercent);
				}
				
				std::string percentPrint;
				
				percentPrint = std::to_string(percent) + mUnity[mCurrentMeasure];
				
				toSetup["ChannelPercent"]("Text") = percentPrint;

				prescale = percent / 100.0f;
			}

			prescale = sqrtf(prescale);
			if (prescale > 0.8f)
			{
				prescale = 0.8f;
			}

			// set ChannnelPercent position depending on where the thumb is in the spiral

			if ((dock.x > 0.45f) && (dock.x < 0.61f))
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.5, 0.0);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.5, 1.0);
			}
			else if (dock.x <= 0.45f)
			{
				toSetup["ChannelPercent"]("Dock") = v2f(1.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.0, 0.5);
			}
			else
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(1.0, 0.5);
			}

			toSetup("PreScale") = v2f(1.44f * prescale, 1.44f * prescale);

			toSetup("Radius") = (float)toSetup("SizeX") * 1.44f * prescale * 0.5f;

			toSetup["ChannelName"]("PreScale") = v2f(1.0f / (1.44f * prescale), 1.0f / (1.44f * prescale));

			toSetup["ChannelPercent"]("FontSize") = 0.6f * 24.0f / prescale;
			toSetup["ChannelPercent"]("MaxWidth") = 0.6f * 100.0f / prescale;

			if (mCurrentMeasure == Percent) 
			{
				if (mUseLikes)
				{
					toSetup["ChannelPercent"]("PreScale") = v2f(0.8f,0.8f);
					toSetup["ChannelPercent"]("FontSize") = 0.6f * 24.0f / prescale;
					toSetup["ChannelPercent"]("MaxWidth") = 0.8f * 100.0f / prescale;
				}
			}


			toSetup["ChannelName"]("FontSize") = 20.0f ;
			toSetup["ChannelName"]("MaxWidth") = 160.0f ;

			const SP<UIImage>& checkTexture = toSetup;

			if(toPlace.second.mName.length())
			{
				if (!checkTexture->HasTexture())
				{
					somethingChanged = true;
					if (toPlace.second.mThumb.mTexture)
					{
						checkTexture->addItem(toPlace.second.mThumb.mTexture);
					}
					else
					{
						LoadUserStruct(toS.second, toPlace.second, true);
					}
				}
			}
			
		}
		toShowCount++;
		if (toShowCount >= mMaxUserCount)
			break;
	}

}

float	TwitterAnalyser::PerAccountUserMap::GetNormalisedSimilitude(const PerAccountUserMap& other)
{
	unsigned int count = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (other.m[i] & m[i])
		{
			count++;
		}
	}

	float result = ((float)count) / ((float)(mSubscribedCount + other.mSubscribedCount - count));

	return result;
}

float	TwitterAnalyser::PerAccountUserMap::GetNormalisedAttraction(const PerAccountUserMap& other)
{
	unsigned int count = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (other.m[i] & m[i])
		{
			count++;
		}
	}

	float minSCount = (float)std::min(mSubscribedCount, other.mSubscribedCount);

	float result = ((float)count) / minSCount;

	return result;
}

void	TwitterAnalyser::prepareForceGraphData()
{

	Histogram<float>	hist({ 0.0,1.0 }, 256);

	mAccountSubscriberMap.clear();

	// for each showed channel
	for (auto& c : mShowedUser)
	{
		if (mUseLikes)
		{
			PerAccountUserMap	toAdd(mCheckedLikersList.size());
			int sindex = 0;
			int subcount = 0;
			for (auto& s : mCheckedLikersList)
			{
				if (isLikerOf(s.first, c.first))
				{
					toAdd.SetSubscriber(sindex);
				}
				++sindex;
			}
			toAdd.mThumbnail = c.second;
			c.second["ChannelPercent"]("Text") = "";
			c.second["ChannelName"]("Text") = "";
			mAccountSubscriberMap[c.first] = toAdd;
			
		}
		else
		{
			PerAccountUserMap	toAdd(mCheckedFollowerList.size());
			int sindex = 0;
			int subcount = 0;
			for (auto& s : mCheckedFollowerList)
			{
				if (isSubscriberOf(s.first, c.first))
				{
					toAdd.SetSubscriber(sindex);
				}
				++sindex;
			}
			toAdd.mThumbnail = c.second;
			c.second["ChannelPercent"]("Text") = "";
			c.second["ChannelName"]("Text") = "";
			mAccountSubscriberMap[c.first] = toAdd;
		}
	}

	for (auto& l1 : mAccountSubscriberMap)
	{
		l1.second.mPos = l1.second.mThumbnail->getValue<v2f>("Dock");
		l1.second.mPos.x = 640 + (rand() % 129) - 64;
		l1.second.mPos.y = 400 + (rand() % 81) - 40;
		l1.second.mForce.Set(0.0f, 0.0f);
		l1.second.mRadius = l1.second.mThumbnail->getValue<float>("Radius");

		int index = 0;
		for (auto& l2 : mAccountSubscriberMap)
		{
			if (&l1 == &l2)
			{
				l1.second.mCoeffs.push_back({ index,-1.0f });
			}
			else
			{
				float coef = l1.second.GetNormalisedSimilitude(l2.second);
				l1.second.mCoeffs.push_back({ index ,coef });
				hist.addValue(coef);
			}
			++index;
		}

		// sort mCoeffs by value
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int, float>& a1, const std::pair<int, float>& a2)
			{
				if (a1.second == a2.second)
				{
					return (a1.first < a2.first);
				}

				return a1.second < a2.second;
			}
		);


		// sort again mCoeffs but by index 
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int, float>& a1, const std::pair<int, float>& a2)
			{
				return a1.first < a2.first;
			}
		);
	}

	hist.normalize();

	std::vector<float> histlimits = hist.getPercentList({ 0.05f,0.5f,0.96f });


	float rangecoef1 = 1.0f / (histlimits[1] - histlimits[0]);
	float rangecoef2 = 1.0f / (histlimits[2] - histlimits[1]);
	int index1 = 0;
	for (auto& l1 : mAccountSubscriberMap)
	{
		// recompute coeffs according to histogram
		int index2 = 0;
		for (auto& c : l1.second.mCoeffs)
		{
			if (index1 != index2)
			{
				// first half ?
				if ((c.second >= histlimits[0]) && (c.second <= histlimits[1]))
				{
					c.second -= histlimits[0];
					c.second *= rangecoef1;
					c.second -= 1.0f;
					c.second = c.second * c.second * c.second;
					c.second += 1.0f;
					c.second *= (histlimits[1] - histlimits[0]);
					c.second += histlimits[0];
				}
				else if ((c.second > histlimits[1]) && (c.second <= histlimits[2]))
				{
					c.second -= histlimits[1];
					c.second *= rangecoef2;
					c.second = c.second * c.second * c.second;
					c.second *= (histlimits[2] - histlimits[1]);
					c.second += histlimits[1];
				}
				c.second -= histlimits[1];
				c.second *= 2.0f;

			}
			++index2;
		}
		++index1;
	}
}

void	TwitterAnalyser::DrawForceBased()
{
	v2f center(1280.0 / 2.0, 800.0 / 2.0);

	const float timeDelay = 10.0f;
	const float timeDivisor = 0.04f;

	float currentTime = ((GetApplicationTimer()->GetTime() - mForcedBaseStartingTime) - timeDelay) * timeDivisor;
	if (currentTime > 1.0f)
	{
		currentTime = 1.0f;
	}
	// first compute attraction force on each thumb
	for (auto& l1 : mAccountSubscriberMap)
	{
		PerAccountUserMap& current = l1.second;

		// always a  bit of attraction to center
		v2f	v(mThumbcenter);
		v -= current.mPos;

		// add a bit of random
		v.x += (rand() % 32) - 16;
		v.y += (rand() % 32) - 16;

		current.mForce *= 0.1f;
		current.mForce = v * 0.001f;

		int i = 0;
		for (auto& l2 : mAccountSubscriberMap)
		{
			if (&l1 == &l2)
			{
				i++;
				continue;
			}

			v2f	v(l2.second.mPos - current.mPos);
			float dist = Norm(v);
			v.Normalize();
			float coef = current.mCoeffs[i].second;
			if (currentTime < 0.0f)
			{
				coef += -currentTime / (timeDelay * timeDivisor);
			}
			if (coef > 0.0f)
			{
				if (dist <= (current.mRadius + l2.second.mRadius)) // if not already touching
				{
					coef = 0.0f;
				}
			}

			current.mForce += v * coef * (l2.second.mRadius + current.mRadius) / ((l2.second.mRadius + current.mRadius) + (dist * dist) * 0.001);

			i++;
		}
	}
	// then move according to forces
	for (auto& l1 : mAccountSubscriberMap)
	{
		PerAccountUserMap& current = l1.second;
		current.mPos += current.mForce;
	}

	if (currentTime > 0.0f)
	{
		// then adjust position with contact
		for (auto& l1 : mAccountSubscriberMap)
		{
			PerAccountUserMap& current = l1.second;

			for (auto& l2 : mAccountSubscriberMap)
			{
				if (&l1 == &l2)
				{
				/*	// check with central image
					v2f	v(current.mPos);
					v -= mThumbcenter;
					float dist = Norm(v);
					v.Normalize();

					float r = (current.mRadius + 120) * currentTime;
					if (dist < r)
					{
						float coef = (r - dist) * 120.0 / (r);
						current.mPos += v * (coef);
					}
					*/
					continue;
				}

				v2f	v(l2.second.mPos - current.mPos);
				float dist = Norm(v);
				v.Normalize();

				float r = (current.mRadius + l2.second.mRadius) * currentTime;
				if (dist < r)
				{
					float coef = (r - dist) * 0.5f;
					current.mPos -= v * coef;
					l2.second.mPos += v * coef;
				}
			}
		}
	}
	// then check if they don't get out of the screen
	for (auto& l1 : mAccountSubscriberMap)
	{

		PerAccountUserMap& current = l1.second;
		v2f	v(current.mPos);
		v -= center;
		float l1 = Norm(v);

		float angle = atan2f(v.y, v.x);
		v.Normalize();

		v2f ellipse(center);
		ellipse.x *= 0.9 * cosf(angle);
		ellipse.y *= 0.9 * sinf(angle);

		v2f tst = ellipse.Normalized();

		ellipse -= tst * current.mRadius;
		float l2 = Norm(ellipse);

		if (l1 > l2)
		{
			current.mPos -= v * (l1 - l2);
		}

		v2f dock(current.mPos);
		dock.x /= 1280.0f;
		dock.y /= 800.0f;

		current.mThumbnail("Dock") = dock;
	}

}

CoreItemSP		TwitterAnalyser::LoadLikersFile(u64 tweetid, const std::string& username)
{
	std::string filename = "Cache/Tweets/" + username + "/" + GetIDString(tweetid) + ".json";
	CoreItemSP likers = LoadJSon(filename);
	return likers;
}
void		TwitterAnalyser::SaveLikersFile(u64 tweetid, const std::string& username)
{
	// likers are stored in unused mFollowers (when in UseLikes mode)
	std::string filename = "Cache/Tweets/" + username + "/" + GetIDString(tweetid) + ".json";

	CoreItemSP likers = CoreItemSP::getCoreVector();
	for (const auto& l : mTweetLikers)
	{
		likers->set("", l);
	}

	SaveJSon(filename, likers, false);
	
}

struct favoriteStruct
{
	u64		tweetID;
	u64		userID;
	u32		likes_count;
	u32		retweet_count;
};

std::vector<favoriteStruct>										mFavorites;
bool	TwitterAnalyser::LoadFavoritesFile(const std::string& username)
{
	std::string filename = "Cache/Tweets/" + username.substr(0,4) + "/" + username + ".favs";

	if (LoadDataFile<favoriteStruct>(filename, mFavorites[username]))
	{
		return true;
	}
	mFavorites[username].clear();
	return false;
}

void	TwitterAnalyser::SaveFavoritesFile(const std::string& username)
{
	std::string filename = "Cache/Tweets/" + username.substr(0, 4) + "/" + username + ".favs";
	SaveDataFile<favoriteStruct>(filename, mFavorites[username]);
}


bool		TwitterAnalyser::LoadFollowingFile(u64 id)
{
	std::string filename = "Cache/Users/"+ GetUserFolderFromID(id)+"/" + GetIDString(id) + ".ids";

	mCurrentFollowing = std::move(LoadIDVectorFile(filename));

	if (mCurrentFollowing.size())
	{
#ifdef LOG_ALL
		writelog("LoadFollowingFile " + std::to_string(id) + "ok");
#endif
		return true;
	}
#ifdef LOG_ALL
	writelog("LoadFollowingFile " + std::to_string(id) + "failed");
#endif
	return false;
}
void		TwitterAnalyser::SaveFollowingFile(u64 id)
{
#ifdef LOG_ALL
	writelog("SaveFollowingFile " + std::to_string(id));
#endif
	std::string filename = "Cache/Users/" + GetUserFolderFromID(id) + "/" + GetIDString(id) + ".ids";
	SaveIDVectorFile(mCurrentFollowing, filename);
}

bool		TwitterAnalyser::LoadFollowersFile()
{

	std::string filename= "Cache/UserName/";
	filename += mUserName + ".ids";
	
	mFollowers = std::move(LoadIDVectorFile(filename));

	if (mFollowers.size())
	{
#ifdef LOG_ALL
		writelog("LoadFollowersFile ok");
#endif
		return true;
	}
#ifdef LOG_ALL
	writelog("LoadFollowersFile failed");
#endif
	return false;
}
void		TwitterAnalyser::SaveFollowersFile()
{
#ifdef LOG_ALL
	writelog("SaveFollowersFile");
#endif
	std::string filename = "Cache/UserName/";
	filename += mUserName + ".ids";

	SaveIDVectorFile(mFollowers, filename);
}

std::vector<u64>		TwitterAnalyser::LoadIDVectorFile(const std::string& filename)
{
	std::vector<u64> loaded;
	if (LoadDataFile<u64>(filename, loaded))
	{
		if (loaded.size() == 0) // following unavailable
		{
			loaded.push_back(mUserID);
		}
	}

	return loaded;
}

bool		TwitterAnalyser::LoadTweetsFile(const std::string& fname)
{
	std::string filename=fname;
	
	if (!fname.length())
	{
		filename = "Cache/UserName/";
		filename += mUserName + ".twts";
	}
	std::vector<Twts>	loaded;
	if (LoadDataFile<Twts>(filename, loaded))
	{
		mTweets = std::move(loaded);
#ifdef LOG_ALL
		writelog("LoadTweetsFile ok");
#endif
		return true;
	}

#ifdef LOG_ALL
	writelog("LoadTweetsFile failed");
#endif
	return false;
}

void		TwitterAnalyser::SaveTweetsFile(const std::string& fname)
{
#ifdef LOG_ALL
	writelog("SaveTweetsFile");
#endif
	std::string filename = fname;

	if (!fname.length())
	{
		filename = "Cache/UserName/";
		filename += mUserName + ".twts";
	}

	SaveDataFile<Twts>(filename, mTweets);
}

void					TwitterAnalyser::SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename)
{
	SaveDataFile<u64>(filename, v);
}

void		TwitterAnalyser::UpdateLikesStatistics()
{
	// check if liker is a follower

	std::set<u64>	followings;
	bool			isMainUserFollower = false;
	if (mDetailedLikeStats && (mHashTag.length()==0)) // when hashtag, no main user
	{
		for (auto following : mCurrentFollowing)
		{
			followings.insert(following);
			if (following == mUserID)
			{
				mLikerFollowerCount++;
				isMainUserFollower = true;
			}
		}
		mCurrentFollowing.clear();
	}
	mUserDetailsAsked.clear();

	std::string user = mTweetLikers[mCurrentTreatedLikerIndex];

	auto itfound = mFavorites.find(user);
	std::vector<favoriteStruct>& currentFavorites = (*itfound).second;
	std::map<u64, float>& currentWeightedFavorites = mWeightedFavorites[user];


	std::map<u64,u64> lFavoritesUsers;
	for (auto f : currentFavorites)
	{
		auto fw = currentWeightedFavorites.find(f.userID);
		if (fw != currentWeightedFavorites.end())
		{
			(*fw).second += 1.0f;
		}
		else
		{
			currentWeightedFavorites[f.userID] = 1.0f;
		}

		lFavoritesUsers[f.userID]=f.userID;
	}

	
	float mainAccountWeight = 1.0f;
	if (!mHashTag.length())
	{
		auto fw = currentWeightedFavorites.find(mUserID);
		if (fw != currentWeightedFavorites.end())
		{
			mainAccountWeight = (*fw).second;
		}
	}

	std::vector<u64>& currentUserLikes=mCheckedLikersList[user];

	for (auto f : lFavoritesUsers)
	{
		currentUserLikes.push_back(f.first);
		
		auto alreadyfound = mFollowersFollowingCount.find(f.first);
		if (alreadyfound != mFollowersFollowingCount.end())
		{
			(*alreadyfound).second.first++;
			if ((*alreadyfound).second.first == (mUserPanelSize/50))
			{
				if (!LoadUserStruct(f.first, (*alreadyfound).second.second, false))
				{
					mUserDetailsAsked.push_back(f.first);
				}
			}
			if (mDetailedLikeStats)
			{
				mFollowersFollowingCount[f.first].second.mLikerCount++;

				mFollowersFollowingCount[f.first].second.mLikerMainUserFollowerCount += isMainUserFollower ? 1 : 0;
				mFollowersFollowingCount[f.first].second.mLikesCount += currentWeightedFavorites[f.first];
				if (followings.find(f.first) != followings.end())
				{
					mFollowersFollowingCount[f.first].second.mLikerFollowerCount++;
					mFollowersFollowingCount[f.first].second.mLikerBothFollowCount += isMainUserFollower ? 1 : 0;
				}
				
			}
		}
		else
		{
			UserStruct	toAdd;
			toAdd.mW = 0.0f;
			if (mDetailedLikeStats)
			{
				toAdd.mLikerCount = 1;
				toAdd.mLikerFollowerCount = 0;
				toAdd.mLikerBothFollowCount = 0;
				toAdd.mLikerMainUserFollowerCount = isMainUserFollower ? 1 : 0;
				toAdd.mLikesCount = currentWeightedFavorites[f.first];
				
				if (followings.find(f.first) != followings.end())
				{
					toAdd.mLikerFollowerCount = 1;
					toAdd.mLikerBothFollowCount = isMainUserFollower ? 1 : 0;
				}

			}

			mFollowersFollowingCount[f.first] = std::pair<unsigned int, UserStruct>(1, toAdd);
		}
	}


	for (auto& toWeight : currentWeightedFavorites)
	{
		float currentW = 1.0f - fabsf(toWeight.second - mainAccountWeight) / (float)(toWeight.second + mainAccountWeight);

		toWeight.second = currentW * mainAccountWeight;
		mFollowersFollowingCount[toWeight.first].second.mW += toWeight.second;
	}
}


void		TwitterAnalyser::UpdateStatistics()
{
	mUserDetailsAsked.clear();
	for (auto f : mCurrentFollowing)
	{
		auto alreadyfound=mFollowersFollowingCount.find(f);
		if (alreadyfound != mFollowersFollowingCount.end())
		{
			(*alreadyfound).second.first++;
			if ((*alreadyfound).second.first == (mUserPanelSize / 50))
			{
				if (!LoadUserStruct(f, (*alreadyfound).second.second, false))
				{
					mUserDetailsAsked.push_back(f);
				}
			}
		}
		else
		{
			UserStruct	toAdd;
			mFollowersFollowingCount[f] = std::pair<unsigned int, UserStruct>(1, toAdd);
		}
	}
}

void	TwitterAnalyser::switchForce()
{
	if (!mDrawForceBased)
	{
		mThumbcenter = (v2f)mMainInterface["thumbnail"]("Dock");
		mThumbcenter.x *= 1280.0f;
		mThumbcenter.y *= 800.0f;

		mMainInterface["thumbnail"]("Dock") = v2f(0.94f, 0.08f);

		prepareForceGraphData();
		mForcedBaseStartingTime = GetApplicationTimer()->GetTime();

		mMainInterface["switchV"]("IsHidden") = true;
		mMainInterface["switchV"]("IsTouchable") = false;

		mMainInterface["switchForce"]("Dock") = v2f( 0.050f,0.950f );

	}
	else
	{
		mMainInterface["thumbnail"]("Dock") = v2f(0.52f, 0.44f);

		mMainInterface["switchV"]("IsHidden") = false;
		mMainInterface["switchV"]("IsTouchable") = true;

		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;
		mMainInterface["switchForce"]("Dock") = v2f(0.950f, 0.050f);
	}
	mDrawForceBased = !mDrawForceBased;

}

void	TwitterAnalyser::treatWebScraperMessage(CoreModifiable* sender, std::string msg)
{
	switch (mScraperState)
	{
	case  GET_LIKES:
	{
		if (msg.find("href") != std::string::npos)
		{
			size_t possibleUserPos = msg.find("twitter.com/");
			if (possibleUserPos != std::string::npos)
			{
				size_t endUserPos = msg.find("\"", possibleUserPos);
				std::string user = msg.substr(possibleUserPos + 12, endUserPos - (possibleUserPos + 12));
				if (user.find("search?") == std::string::npos) // seams OK
				{
					if (mCurrentScrappedUserNameList.size())
					{
						if (mCurrentScrappedUserNameList.back().userName == user)
						{
							mCurrentScrappedUserNameList.back().foundCount++;
						}
						else
						{
							mCurrentScrappedUserNameList.push_back({ user,1 });
						}
					}
					else
					{
						mCurrentScrappedUserNameList.push_back({ user,1 });
					}
				}
			}
		}

		if ((msg.find("scriptDone") != std::string::npos) || (msg=="null"))
		{
			if (mCurrentScrappedUserNameList.size()) // some users were found ?
			{
				std::vector<std::string>	validscrappedlist;

				for (const auto& u : mCurrentScrappedUserNameList)
				{
					if (u.foundCount == 2)
					{
						validscrappedlist.push_back(u.userName);
					}
				}

				bool valid = true;
				if (mTweetLikers.size())
				{
					if (validscrappedlist.back() == mTweetLikers.back())
					{
						valid = false;
					}
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

					if (mTweetLikers.size() && oneWasAdded)
					{
						mCurrentScrappedUserNameList.clear();
						mNextScript = "var toscroll=document.querySelector('[href=\"/" + mTweetLikers.back() + "\"]');"\
							"toscroll.scrollIntoView(true);"\
							"window.chrome.webview.postMessage(\"scriptDone\");";
						LaunchScript(sender);
						mScraperState = SCROLL_LIKES;
					}
				}
			}
			
			if (mScraperState != SCROLL_LIKES) // no more likers found
			{
				u64 tweetID = mTweets[mCurrentTreatedTweetIndex].mTweetID;
				mTweetLikersMap.clear();
				randomizeVector(mTweetLikers);

				std::string username = mUserName;
				if (mHashTag.length())
				{
					username = mTweetsPosters[mCurrentTreatedTweetIndex];
				}
				
				SaveLikersFile(tweetID,username);
				mState = GET_USER_FAVORITES;
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

void	TwitterAnalyser::LaunchScript(CoreModifiable* sender)
{
	if (mNextScript != "")
	{
		sender->setValue("Script",(UTF8Char*)mNextScript.c_str());
		mNextScript == "";
	}
}

