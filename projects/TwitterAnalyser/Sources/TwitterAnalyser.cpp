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
#include "UI/UITexture.h"

IMPLEMENT_CLASS_INFO(TwitterAnalyser);

IMPLEMENT_CONSTRUCTOR(TwitterAnalyser)
{
	
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

void	TwitterAnalyser::ProtectedInit()
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

	mUserName = initP["UserName"];
	auto SetMemberFromParam = [&](auto& x, const char* id) {
		if (!initP[id].isNil()) x = initP[id];
	};

	SetMemberFromParam(mUserPanelSize, "UserPanelSize");
	SetMemberFromParam(mMaxUserCount, "MaxUserCount");
	SetMemberFromParam(mValidUserPercent, "ValidUserPercent");
	SetMemberFromParam(mWantedTotalPanelSize, "WantedTotalPanelSize");

	int oldFileLimitInDays = 3 * 30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");

	mOldFileLimit = 60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays;

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


	// search if current user is in Cached data
	std::string currentUserProgress = "Cache/UserName/";
	currentUserProgress += mUserName + ".json";
	CoreItemSP currentP = LoadJSon(currentUserProgress);

	if (currentP.isNil()) // new user
	{
		// check classic User Cache

		std::string url = "1.1/users/show.json?screen_name=" + mUserName;
		mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
		mAnswer->AddHeader(mTwitterBear[NextBearer()]);
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
		mState = 1;
		
	}

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
	mLastUpdate = GetApplicationTimer()->GetTime();
}

void	TwitterAnalyser::ProtectedUpdate()
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

		switch (mState)
		{
		case 0: // wait
			break;

		case 1: // get follower list
		{
			if (mFollowers.size() == 0)
			{
				// try to load followers file
				if (LoadFollowersFile() && (mFollowersNextCursor == "-1"))
				{
					mState = 22;
					break;
				}
			}
		}
		case 11:
		{
			mState = 11;
			// check that we reached the end of followers
			if ((mFollowers.size() == 0) || (mFollowersNextCursor != "-1"))
			{

				if (CanLaunchRequest())
				{
					// need to ask more data
					std::string url = "1.1/followers/ids.json?cursor=" + mFollowersNextCursor + "&screen_name=" + mUserName + "&count=5000";
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFollowers", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->Init();
					myRequestCount++;
					mState = 0;
					RequestLaunched(60.5);
				}
			}

			break;
		}
		case 22: // check fake
		{
			u64 currentFollowerID = mFollowers[mTreatedFollowerIndex];
			UserStruct	tmpuser;
			if (!LoadUserStruct(currentFollowerID, tmpuser, false))
			{
				mUserDetailsAsked.push_back(currentFollowerID);
				mState = 44;	// ask for user detail and come back here
			}
			else // check if follower seems fake
			{
				int creationYear = getCreationYear(tmpuser.UTCTime);
				int deltaYear = 1+(mCurrentYear - creationYear);

				if ((tmpuser.mFollowersCount < 4) && (tmpuser.mFollowingCount < 30) && ((tmpuser.mStatuses_count/deltaYear)<3) ) // this is FAKE NEEEWWWWSS !
				{
					mFakeFollowerCount++;
					NextTreatedFollower();
					if ((mTreatedFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
					{
						mState = 10;
						break;
					}
				}
				else
				{
					mState = 2; // this follower is OK, treat it
				}
			}
		}
		break;
		case 2: // treat one follower
		{
			
			// search for an available next-cursor for current following
			u64 currentFollowerID = mFollowers[mTreatedFollowerIndex];
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
			LoadFollowingFile(currentFollowerID);

			if (!needRequest)
			{
				mState = 3;
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
					mAnswer->Init();
					myRequestCount++;
					mState = 0;
					RequestLaunched(60.5);
				}
			}

			break;
		}
		case 3: // update statistics
		{
			// if only one followning, just skip ( probably 
			if (mCurrentFollowing.size() > 1)
			{
				u64 currentFollowerID = mFollowers[mTreatedFollowerIndex];
				mValidFollowerCount++;
				UpdateStatistics();
			}
			mCurrentFollowing.clear();

			if (mUserDetailsAsked.size()) // can only go there when validFollower is true
			{
				mState = 4;
				break;
			}
			NextTreatedFollower();
			if ( (mTreatedFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
			{
				mState = 10;
				break;
			}

			mState = 22;

			break;
		}
		case 444: // wait for 
			break;
		case 44:
		case 4: // update users details
		{
			if (mUserDetailsAsked.size())
			{
				if (CanLaunchRequest())
				{
					std::string url = "1.1/users/show.json?user_id=" + GetIDString(mUserDetailsAsked.back());
					mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
					mAnswer->AddHeader(mTwitterBear[NextBearer()]);
					mAnswer->Init();
					myRequestCount++;
					mUserDetailsAsked.pop_back();
					RequestLaunched(1.1);
					if(mState==44) // if state is 
						mState = 444;
				}
			}
			else
			{
				if (mState == 4) // update statistics was already done, just go to next
				{
					NextTreatedFollower(); 
					if ((mValidFollowerCount == mFollowers.size()) || (mValidFollowerCount == mUserPanelSize))
					{
						mState = 10;
						break;
					}
				}
				mState = 22;
			}

			break;
		}
		case 10:
		{
			// done, todo
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
			sprintf(textBuffer, "Treated Users : %d", mValidFollowerCount);
			mMainInterface["TreatedFollowers"]("Text") = textBuffer;
			sprintf(textBuffer, "Found followings : %d", mFollowersFollowingCount.size());
			mMainInterface["FoundFollowings"]("Text") = textBuffer;
			sprintf(textBuffer, "Inactive Followers : %d", mFakeFollowerCount);
			mMainInterface["FakeFollowers"]("Text") = textBuffer;

			if (mState != 10)
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

			// check if Channel texture was loaded

			if (mCurrentUser.mThumb.mTexture && mMainInterface["thumbnail"])
			{
				const SP<UITexture>& tmp = mMainInterface["thumbnail"];

				if (!tmp->GetTexture())
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
	DataDrivenBaseApplication::ProtectedClose();
}

void	TwitterAnalyser::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
	}
}
void	TwitterAnalyser::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		
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
					if (code == 88)
					{
						mWaitQuota = true;
						mWaitQuotaCount++;
						mStartWaitQuota = GetApplicationTimer()->GetTime();
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

CoreItemSP	TwitterAnalyser::LoadJSon(const std::string& fname, bool utf16)
{
	auto& pathManager = KigsCore::Singleton<FilePathManager>();
	SmartPointer<::FileHandle> filenamehandle = pathManager->FindFullName(fname);

	// Windows specific code 
#ifdef WIN32

	if ((filenamehandle->mStatus & FileHandle::Exist) && (mOldFileLimit > 1.0))
	{
		struct _stat resultbuf;

		if (_stat(filenamehandle->mFullFileName.c_str(), &resultbuf) == 0)
		{
			auto mod_time = resultbuf.st_mtime;

			double diffseconds = difftime(mCurrentTime, mod_time);

			if (diffseconds > mOldFileLimit)
			{
				return nullptr;
			}
		}
	}


#endif
	CoreItemSP initP;
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
			mState = 11;
			mFollowersNextCursor = nextStr;
		}
		else
		{
			mState = 22;
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

DEFINE_METHOD(TwitterAnalyser, getFollowing)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		u64 currentID=sender->getValue<u64>("UserID");
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
		if (nextStr == "0")
		{
			nextStr = "-1";
		}
		currentP->set("next-cursor", nextStr);

		SaveJSon(filename, currentP);

		mState = 2;
	}
	else
	{
		if (!mWaitQuota)
		{
			// this user is not available, go to next one
			u64 currentID = sender->getValue<u64>("UserID");
			SaveFollowingFile(currentID);

			std::string stringID = GetIDString(currentID);
			std::string filename = "Cache/Users/" + GetUserFolderFromID(currentID) + "/" + stringID + "_nc.json";

			CoreItemSP currentP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();

			std::string nextStr = "-1";
			currentP->set("next-cursor", nextStr);

			SaveJSon(filename, currentP);

			mState = 3;
		}
	}

	return true;
}


DEFINE_METHOD(TwitterAnalyser, getUserDetails)
{
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		u64 currentID= json["id"];

		UserStruct	tmp;
		UserStruct* pUser = &tmp;


		if (json["screen_name"] == mUserName)
		{
			mUserID = json["id"];
			pUser = &mCurrentUser;
		}
		else if(mState==4)
		{
			pUser = &mFollowersFollowingCount[currentID].second;
		}
		else if(mFollowersFollowingCount.find(currentID) != mFollowersFollowingCount.end())
		{
			pUser = &mFollowersFollowingCount[currentID].second;
		}

		pUser->mName = json["screen_name"];
		pUser->mFollowersCount = json["followers_count"];
		pUser->mFollowingCount = json["friends_count"];
		pUser->mStatuses_count = json["statuses_count"];
		pUser->UTCTime = json["created_at"];
		pUser->mThumb.mURL = CleanURL(json["profile_image_url_https"]);

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
		if (json["screen_name"] == mUserName)
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
			mState = 1;
		}
		if (mState == 444)
		{
			mState = 44;
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
	mShowInfluence = !mShowInfluence;
	printf("switch precent / inspiration\n");
}

std::string	TwitterAnalyser::GetUserFolderFromID(u64 id)
{
	std::string result;

	for (int i = 0; i < 5; i++)
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
	std::string filename = "Cache/Thumbs/";
	filename += GetIDString(id);
	filename += ".jpg";

	auto& pathManager = KigsCore::Singleton<FilePathManager>();
	SmartPointer<::FileHandle> fullfilenamehandle = pathManager->FindFullName(filename);

	if (fullfilenamehandle->mStatus & FileHandle::Exist)
	{
		auto& textureManager = KigsCore::Singleton<TextureFileManager>();
		ch.mThumb.mTexture = textureManager->GetTexture(filename);
		return true;
	}
	filename = "Cache/Thumbs/";
	filename += GetIDString(id);
	filename += ".png";
	fullfilenamehandle = pathManager->FindFullName(filename);

	if (fullfilenamehandle->mStatus & FileHandle::Exist)
	{
		auto& textureManager = KigsCore::Singleton<TextureFileManager>();
		ch.mThumb.mTexture = textureManager->GetTexture(filename);
		return true;
	}

	filename = "Cache/Thumbs/";
	filename += GetIDString(id);
	filename += ".gif";
	fullfilenamehandle = pathManager->FindFullName(filename);

	if (fullfilenamehandle->mStatus & FileHandle::Exist)
	{
		auto& textureManager = KigsCore::Singleton<TextureFileManager>();
		ch.mThumb.mTexture = textureManager->GetTexture(filename);
		return true;
	}

	return false;
}

void	TwitterAnalyser::refreshAllThumbs()
{
	if (mValidFollowerCount < 10)
		return;

	bool somethingChanged = false;

	float wantedpercent = mValidUserPercent;

	// get all parsed channels and get the ones with more than mValidChannelPercent subscribed users
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

	if (mShowInfluence) // normalize according to follower count
	{
		std::sort(toShow.begin(), toShow.end(), [this](const std::pair<unsigned int, u64>& a1, const std::pair<unsigned int, u64>& a2)
			{

				auto&  a1User = mFollowersFollowingCount[a1.second];
				auto&  a2User = mFollowersFollowingCount[a2.second];

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
	else
	{
		std::sort(toShow.begin(), toShow.end(), [](const std::pair<unsigned int, u64>& a1, const std::pair<unsigned int, u64>& a2)
			{
				if (a1.first == a2.first)
				{
					return a1.second > a2.second;
				}
				return (a1.first > a2.first);
			}
		);
	}

	// check for channels already shown / to remove

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[tos.second] = 1;
		toShowCount++;
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
			if (mShowInfluence) // normalize according to follower count
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
				toSetup["ChannelPercent"]("Text") = std::to_string((int)(k)) + "sc";

				prescale = 1.5f * k / 100.0f;

				if (prescale < 0.1f)
				{
					prescale = 0.1f;
				}
			}
			else
			{
				int percent = (int)(100.0f * ((float)toPlace.first / (float)mValidFollowerCount));
				toSetup["ChannelPercent"]("Text") = std::to_string(percent) + "%";

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

			toSetup("PreScaleX") = 1.44f * prescale;
			toSetup("PreScaleY") = 1.44f * prescale;

			toSetup["ChannelName"]("PreScaleX") = 1.0f / (1.44f * prescale);
			toSetup["ChannelName"]("PreScaleY") = 1.0f / (1.44f * prescale);

			toSetup["ChannelPercent"]("FontSize") = 0.6f * 24.0f / prescale;
			toSetup["ChannelPercent"]("MaxWidth") = 0.6f * 100.0f / prescale;
			toSetup["ChannelName"]("FontSize") = 20.0f ;
			toSetup["ChannelName"]("MaxWidth") = 160.0f ;

			const SP<UITexture>& checkTexture = toSetup;

			if(toPlace.second.mName.length())
			{
				if (!checkTexture->GetTexture())
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

bool		TwitterAnalyser::LoadFollowingFile(u64 id)
{
	std::string filename = "Cache/Users/"+ GetUserFolderFromID(id)+"/" + GetIDString(id) + ".ids";

	mCurrentFollowing = std::move(LoadIDVectorFile(filename));

	if (mCurrentFollowing.size())
	{
		return true;
	}
	return false;
}
void		TwitterAnalyser::SaveFollowingFile(u64 id)
{
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
		return true;
	}
	return false;
}
void		TwitterAnalyser::SaveFollowersFile()
{
	std::string filename = "Cache/UserName/";
	filename += mUserName + ".ids";

	SaveIDVectorFile(mFollowers, filename);
}

std::vector<u64>		TwitterAnalyser::LoadIDVectorFile(const std::string& filename)
{
	std::vector<u64> loaded;

	SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "rb");
	if (L_File->mFile)
	{
		// get file size
		Platform_fseek(L_File.get(), 0, SEEK_END);
		long filesize = Platform_ftell(L_File.get());
		Platform_fseek(L_File.get(), 0, SEEK_SET);

		loaded.resize(filesize / sizeof(u64));

		Platform_fread(loaded.data(), sizeof(u64), loaded.size() , L_File.get());
		Platform_fclose(L_File.get());

		if (loaded.size() == 0) // following unavailable
		{
			loaded.push_back(mUserID);
		}

	}

	return loaded;
}
void					TwitterAnalyser::SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename)
{
	SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
	if (L_File->mFile)
	{
		Platform_fwrite(v.data(), 1,v.size()*sizeof(u64), L_File.get());
		Platform_fclose(L_File.get());
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
			if ((*alreadyfound).second.first == 3)
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