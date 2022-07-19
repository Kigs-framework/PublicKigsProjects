#include "TwitterConnect.h"
#include "JSonFileParser.h"
#include "HTTPRequestModule.h"
#include "TextureFileManager.h"
#include "TinyImage.h"
#include "JPEGClass.h"
#include "PNGClass.h"
#include "GIFClass.h"

double		TwitterConnect::mOldFileLimit=0;
time_t		TwitterConnect::mCurrentTime=0;

std::map<u32, std::pair<u64, u32>>			TwitterConnect::mDateToTweet;

TwitterConnect::datestruct	TwitterConnect::mDates[2];
bool						TwitterConnect::mUseDates = false;

TwitterConnect* TwitterConnect::mInstance = nullptr;

IMPLEMENT_CLASS_INFO(TwitterConnect)

void	TwitterConnect::initBearer(CoreItemSP initP)
{
	// retreive parameters
	CoreItemSP foundBear;
	int bearIndex = 0;
	do
	{
		char	BearName[64];
		++bearIndex;
		sprintf(BearName, "TwitterBear%d", bearIndex);

		foundBear = initP[(std::string)BearName];

		if (foundBear)
		{
			mTwitterBear.push_back("authorization: Bearer " + (std::string)foundBear);

		}
	} while (foundBear);
}

bool TwitterConnect::checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle, double OldFileLimit)
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

void	TwitterConnect::FilterTweets(u64 authorid,std::vector<TwitterConnect::Twts>& twts, bool excludeRT, bool excludeReplies)
{
	if (!(excludeRT || excludeReplies)) // nothing to exclude
	{
		return;
	}

	std::vector<TwitterConnect::Twts> finallist;
	finallist.reserve(twts.size());
	for (const auto& t : twts)
	{
		bool exludethisone = false;
		if (excludeRT)
		{
			if (t.mAuthorID != authorid)
			{
				exludethisone = true;
			}
		}
		if (excludeReplies)
		{
			if (t.mFlag)
			{
				exludethisone = true;
			}
		}
		if (!exludethisone)
		{
			finallist.push_back(t);
		}
	}

	twts=std::move(finallist);

}


void		TwitterConnect::SaveJSon(const std::string& fname, const CoreItemSP& json, bool utf16)
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


CoreItemSP	TwitterConnect::LoadJSon(const std::string& fname, bool useOldFileLimit, bool utf16)
{
	SmartPointer<::FileHandle> filenamehandle;

	CoreItemSP initP(nullptr);
	if (checkValidFile(fname, filenamehandle, useOldFileLimit?mOldFileLimit:0.0))
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

TwitterConnect::TwitterConnect(const kstl::string& name, CLASS_NAME_TREE_ARG) : CoreModifiable(name, PASS_CLASS_NAME_TREE_ARG)
{
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;
	mCurrentTime = time(0);

	TwitterConnect::mInstance = this;

	mDates[0].dateAsInt = mDates[1].dateAsInt = 0;
	mDates[0].dateAsString = mDates[1].dateAsString = "";

}

void	TwitterConnect::initConnection(double oldfiletime)
{
	mOldFileLimit = oldfiletime;


	// init twitter connection
	mTwitterConnect = KigsCore::GetInstanceOf("TwitterConnect", "HTTPConnect");
	mTwitterConnect->setValue("HostName", "api.twitter.com");
	mTwitterConnect->setValue("Type", "HTTPS");
	mTwitterConnect->setValue("Port", "443");
	mTwitterConnect->Init();
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

void		TwitterConnect::LaunchDownloader(u64 id, UserStruct& ch)
{
	// first, check that downloader for the same thumb is not already launched

	for (const auto& d : TwitterConnect::mInstance->mDownloaderList)
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
	KigsCore::Connect(CurrentDownloader.get(), "onDownloadDone", TwitterConnect::mInstance, "thumbnailReceived");

	TwitterConnect::mInstance->mDownloaderList.push_back({ CurrentDownloader , {id,&ch} });

	CurrentDownloader->Init();
}

std::string	TwitterConnect::GetUserFolderFromID(u64 id)
{
	std::string result;

	for (int i = 0; i < 4; i++)
	{
		result += '0' + (id % 10);
		id = id / 10;
	}

	return result;
}

std::string  TwitterConnect::CleanURL(const std::string& url)
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

std::string	TwitterConnect::GetIDString(u64 id)
{
	char	idstr[64];
	sprintf(idstr, "%llu", id);

	return idstr;
}

// load json channel file dans fill ChannelStruct
bool		TwitterConnect::LoadUserStruct(u64 id, UserStruct& ch, bool requestThumb)
{
	std::string filename = "Cache/Users/";

	std::string folderName = GetUserFolderFromID(id);

	filename += folderName + "/" + GetIDString(id) + ".json";

	CoreItemSP initP = LoadJSon(filename, true,true);

	if (!initP) // file not found, return
	{
		return false;
	}
#ifdef LOG_ALL
	writelog("loaded user" + std::to_string(id) + "details");
#endif
	ch.mName = initP["Name"];
	ch.mID = id;
	ch.mFollowersCount = initP["FollowersCount"];
	ch.mFollowingCount = initP["FollowingCount"];
	ch.mStatuses_count = initP["StatusesCount"];
	ch.UTCTime = initP["CreatedAt"]->toString();
	if (initP["ImgURL"])
	{
		ch.mThumb.mURL = initP["ImgURL"]->toString();
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

void		TwitterConnect::SaveTweet(const usString& text, u64 authorID, u64 tweetID)
{
#ifdef SAVE_TWEETS
	JSonFileParserUTF16 L_JsonParser;
	CoreItemSP initP = MakeCoreMapUS();
	initP->set("text", text);

	std::string folderName = GetUserFolderFromID(authorID);

	std::string filename = "Cache/Users/";
	filename += folderName + "/" + GetIDString(authorID) + "/Tweets/" + GetIDString(tweetID) + ".json";

	L_JsonParser.Export((CoreMap<usString>*)initP.get(), filename);
#endif
}


void		TwitterConnect::SaveUserStruct(u64 id, UserStruct& ch)
{
#ifdef LOG_ALL
	writelog("save user" + std::to_string(id) + "details");
#endif

	JSonFileParserUTF16 L_JsonParser;
	CoreItemSP initP = MakeCoreMapUS();
	initP->set("Name", ch.mName);
	initP->set("FollowersCount", (int)ch.mFollowersCount);
	initP->set("FollowingCount", (int)ch.mFollowingCount);
	initP->set("StatusesCount", (int)ch.mStatuses_count);
	initP->set("CreatedAt", ch.UTCTime);

	if (ch.mThumb.mURL != "")
	{
		initP->set("ImgURL", (usString)ch.mThumb.mURL);
	}

	std::string folderName = GetUserFolderFromID(id);

	std::string filename = "Cache/Users/";
	filename += folderName + "/" + GetIDString(id) + ".json";

	L_JsonParser.Export((CoreMap<usString>*)initP.get(), filename);
}

bool		TwitterConnect::LoadThumbnail(u64 id, UserStruct& ch)
{
	SmartPointer<::FileHandle> fullfilenamehandle;

	std::vector<std::string> ExtensionList = { ".jpg",".png" ,".gif" };

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

// user fname if given
// else construct filename with username
bool	TwitterConnect::LoadTweetsFile(std::vector<Twts>& tweetlist, const std::string& username, const std::string& fname)
{
	std::string filename = fname;

	if (!fname.length())
	{
		filename = "Cache/UserName/";
		if (mUseDates)
		{
			filename += "_" + mDates[0].dateAsString + "_" + mDates[1].dateAsString + "_";
		}
		filename += username + ".twts";
	}
	std::vector<Twts>	loaded;
	// for dated search, don't use old file limit here
	if (LoadDataFile<Twts>(filename, loaded))
	{
		tweetlist = std::move(loaded);

		return true;
	}

	return false;
}

void	TwitterConnect::SaveTweetsFile(const std::vector<Twts>& tweetlist, const std::string& username, const std::string& fname)
{
	std::string filename = fname;

	if (!fname.length())
	{
		filename = "Cache/UserName/";
		if (mUseDates)
		{
			filename += "_" + mDates[0].dateAsString + "_" + mDates[1].dateAsString + "_";
		}
		filename += username + ".twts";
	}

	SaveDataFile<Twts>(filename, tweetlist);
}

bool			TwitterConnect::LoadFavoritesFile(u64 userid, std::vector<TwitterConnect::Twts>& fav)
{
	std::string filename = "Cache/Users/" + GetUserFolderFromID(userid) + "/" + GetIDString(userid) ;

	if (mUseDates)
	{
		filename += "_" + mDates[0].dateAsString + "_" + mDates[1].dateAsString + "_";
	}
	filename += ".favs";
	// for dated search, don't use old file limit here
	if (LoadDataFile<TwitterConnect::Twts>(filename, fav))
	{
		return true;
	}
	fav.clear();
	return false;
}


bool		TwitterConnect::LoadFavoritesFile(const std::string& username, std::vector<TwitterConnect::Twts>& fav)
{
	std::string filename = "Cache/Tweets/" + username.substr(0, 4) + "/";
		
	if (mUseDates)
	{
		filename += "_" + mDates[0].dateAsString + "_" + mDates[1].dateAsString + "_";
	}
	filename += username + ".favs";
	// for dated search, don't use old file limit here
	if (LoadDataFile<TwitterConnect::Twts>(filename, fav))
	{
		return true;
	}
	fav.clear();
	return false;
}

void		TwitterConnect::SaveFavoritesFile(u64 userid, const std::vector<TwitterConnect::Twts>& favs)
{
	std::string filename = "Cache/Users/" + GetUserFolderFromID(userid) + "/" + GetIDString(userid);

	if (mUseDates)
	{
		filename += "_" + mDates[0].dateAsString + "_" + mDates[1].dateAsString + "_";
	}
	filename += ".favs";

	SaveDataFile<TwitterConnect::Twts>(filename, favs);
}


void		TwitterConnect::SaveFavoritesFile(const std::string& username, const std::vector<TwitterConnect::Twts>& favs)
{
	std::string filename = "Cache/Tweets/" + username.substr(0, 4) + "/";
	if (mUseDates)
	{
		filename += "_" + mDates[0].dateAsString + "_" + mDates[1].dateAsString + "_";
	}
	filename += username + ".favs";

	SaveDataFile<TwitterConnect::Twts>(filename, favs);
}

bool TwitterConnect::LoadLikersFile(u64 tweetid, std::vector<u64>& likers)
{
	std::string filename = "Cache/Tweets/" + GetUserFolderFromID(tweetid) + "/" + GetIDString(tweetid) + ".likers";

	// if dated search, then don't use old file limit here ?
	bool result=LoadDataFile<u64>(filename, likers);
	
	return result;
}

bool TwitterConnect::LoadRetweetersFile(u64 tweetid, std::vector<u64>& rttwers)
{
	std::string filename = "Cache/Tweets/" + GetUserFolderFromID(tweetid) + "/" + GetIDString(tweetid) + ".retweeters";

	// if dated search, then don't use old file limit here ?
	bool result=LoadDataFile<u64>(filename, rttwers);

	return result;
}

void			TwitterConnect::SaveRetweetersFile(const std::vector<u64>& RTers, u64 tweetid)
{
	std::string filename = "Cache/Tweets/" + GetUserFolderFromID(tweetid) + "/" + GetIDString(tweetid) + ".retweeters";

	SaveDataFile<u64>(filename, RTers);
}

CoreItemSP		TwitterConnect::LoadLikersFile(u64 tweetid, const std::string& username)
{
	std::string filename = "Cache/Tweets/" + username + "/" + GetIDString(tweetid) + ".json";

	// if dated search, then don't use old file limit here ?
	CoreItemSP likers = LoadJSon(filename);
	return likers;
}

void		TwitterConnect::SaveLikersFile(const std::vector<u64>& tweetLikers, u64 tweetid)
{
	std::string filename = "Cache/Tweets/" + GetUserFolderFromID(tweetid) + "/" + GetIDString(tweetid) + ".likers";

	SaveDataFile<u64>(filename, tweetLikers);
}

void		TwitterConnect::SaveLikersFile(const std::vector<std::string>& tweetLikers,u64 tweetid, const std::string& username)
{
	std::string filename = "Cache/Tweets/" + username + "/" + GetIDString(tweetid) + ".json";

	CoreItemSP likers = MakeCoreVector();
	for (const auto& l : tweetLikers)
	{
		likers->set("", l);
	}

	SaveJSon(filename, likers, false);

}

void	TwitterConnect::launchGenericRequest(float waitDelay)
{
	mAnswer->AddHeader(mTwitterBear[NextBearer()]);
	mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
	mAnswer->AddDynamicAttribute<maFloat, float>("WaitDelay", waitDelay);

	double waitdelay = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime;
	mNextRequestDelay -= waitdelay;

	if (mNextRequestDelay < 0.0)
	{
		mAnswer->Init();
		mRequestCount++;
		RequestLaunched(waitDelay);
	}
	else
	{
		Upgrade("DelayedFuncUpgrador");
		setValue("DelayedFunction", "sendRequest");
		setValue("Delay", mNextRequestDelay);
	}
}


void	TwitterConnect::launchGetFavoritesRequest(u64 userid, const std::string& nextCursor)
{
	// use since ID, max ID here to retrieve tweets in a given laps of time
	std::string url = "2/users/" + GetIDString(userid) + "/liked_tweets?expansions=author_id,referenced_tweets.id.author_id&tweet.fields=author_id,public_metrics,created_at,text,referenced_tweets";

	// can't restrict to date for favorites with API V2
/*	if (mDates[0].dateAsInt && mDates[1].dateAsInt)
	{
		url += "&start_time=" + mDates[0].dateAsString + "T00:00:00Z";
		url += "&end_time=" + mDates[1].dateAsString + "T23:59:59Z";
	}*/

	url += "&max_results=100";

	if (nextCursor != "-1")
	{
		url += "&pagination_token=" + nextCursor;
	}

	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFavorites", this);

	// 75 req per 15 minutes
	launchGenericRequest(12.5);
}

void	TwitterConnect::launchSearchTweetRequest(const std::string& hashtag, const std::string& nextCursor)
{
	std::string url = "2/tweets/search/recent?query=" + getHashtagURL(hashtag) + "&expansions=author_id,referenced_tweets.id,referenced_tweets.id.author_id&tweet.fields=author_id,public_metrics,created_at,text,referenced_tweets";

	if (mDates[0].dateAsInt && mDates[1].dateAsInt)
	{
		url += "&start_time=" + mDates[0].dateAsString + "T00:00:00Z";
		url += "&end_time=" + mDates[1].dateAsString + "T23:59:59Z";
	}

	url += "&max_results=100";

	if (nextCursor != "-1")
	{
		url += "&next_token="+nextCursor;
	}
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getTweets", this);

	// 450 req per 15 minutes
	launchGenericRequest(2.5);

}


void	TwitterConnect::launchGetTweetRequest(u64 userid,const std::string& username,  const std::string& nextCursor)
{
	std::string url = "2/users/" + std::to_string(userid) + "/tweets?expansions=author_id,referenced_tweets.id.author_id&tweet.fields=author_id,public_metrics,created_at,text,referenced_tweets";

	if (mDates[0].dateAsInt && mDates[1].dateAsInt)
	{
		url += "&start_time=" + mDates[0].dateAsString + "T00:00:00Z";
		url += "&end_time=" + mDates[1].dateAsString + "T23:59:59Z";
	}

	url += "&max_results=100";
	
	if (nextCursor != "-1")
	{
		url += "&pagination_token=" + nextCursor;
	}
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getTweets", this);

	// 1500 req per 15 minutes
	launchGenericRequest(1.0);
}

void TwitterConnect::launchUserDetailRequest(const std::string& UserName, UserStruct& ch)
{

	// check classic User Cache
	std::string url = "2/users/by/username/" + UserName + "?user.fields=created_at,public_metrics,profile_image_url";
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);

	// 900 req per 15 minutes
	launchGenericRequest(1.5);

}

void TwitterConnect::launchUserDetailRequest(u64 UserID, UserStruct& ch)
{

	// check classic User Cache
	std::string url = "2/users/" + std::to_string(UserID) + "?user.fields=created_at,public_metrics,profile_image_url";
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
	mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::ULONG,"UserID", UserID);

	// 900 req per 15 minutes
	launchGenericRequest(1.5);
	
}
void	TwitterConnect::launchGetLikers(u64 tweetid, const std::string& nextToken)
{
	std::string url = "2/tweets/" + std::to_string(tweetid) + "/liking_users";

	url += "?max_results=100";
	if (nextToken != "-1")
	{
		url += "&pagination_token=" + nextToken;
	}
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getLikers", this);

	// 75 req per 15 minutes
	launchGenericRequest(12.5);
}


void	TwitterConnect::launchGetRetweeters(u64 tweetid, const std::string& nextToken)
{
	std::string url = "2/tweets/" + std::to_string(tweetid) + "/retweeted_by";

	url += "?max_results=100";
	if (nextToken != "-1")
	{
		url += "&pagination_token=" + nextToken;
	}
	// warning use same callback as getLikers
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getLikers", this);

	// 75 req per 15 minutes
	launchGenericRequest(12.5);
}


void	TwitterConnect::launchGetFollow(u64 userid,const std::string& followtype, const std::string& nextToken)
{

	std::string url = "2/users/"+ GetIDString(userid) +"/"+ followtype +"?max_results=1000";
	if (nextToken != "-1")
	{
		url += "&pagination_token=" + nextToken;
	}

	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFollow", this);

	// 1 req per minute
	launchGenericRequest(60.5);

}

std::vector<u64>		TwitterConnect::LoadIDVectorFile(const std::string& filename,bool &fileExist,bool useoldfilelimit)
{
	fileExist = false;
	std::vector<u64> loaded;
	if (LoadDataFile<u64>(filename, loaded, useoldfilelimit))
	{
		fileExist = true;
	}

	return loaded;
}

bool		TwitterConnect::LoadFollowFile(u64 id, std::vector<u64>& following, const std::string& followtype)
{
	std::string filename = "Cache/Users/" + GetUserFolderFromID(id) + "/" + GetIDString(id) + "_" + followtype + ".ids";
	bool fileexist = false;
	following = LoadIDVectorFile(filename, fileexist);
	return fileexist;
}

void		TwitterConnect::SaveFollowFile(u64 id, const std::vector<u64>& v, const std::string& followtype)
{
	std::string filename = "Cache/Users/" + GetUserFolderFromID(id) + "/" + GetIDString(id) + "_" + followtype + ".ids";
	SaveIDVectorFile(v, filename);
}

void	TwitterConnect::SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename)
{
	SaveDataFile<u64>(filename, v);
}


CoreItemSP	TwitterConnect::RetrieveJSON(CoreModifiable* sender)
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
			printf("");
		}
		else
		{

			usString	utf8string((UTF8Char*)validstring.c_str());

			JSonFileParserUTF16 L_JsonParser;
			CoreItemSP result = L_JsonParser.Get_JsonDictionaryFromString(utf8string);

			if (result["title"])
			{
				std::string t(result["title"]);
				if (t == "Too Many Requests")
				{
					Upgrade("DelayedFuncUpgrador");
					setValue("DelayedFunction", "resendRequest");
					setValue("Delay", 120.0f); // wait 2 minutes
					mWaitQuota = true;
					mWaitQuotaCount++;
					return nullptr;
				}
			}

			if (result["error"])
			{
				return nullptr;
			}
			if (result["errors"])
			{
				if (result["errors"][0]["code"])
				{
					int code = result["errors"][0]["code"];
					mApiErrorCode = code;
					if (code == 88)
					{
						Upgrade("DelayedFuncUpgrador");
						setValue("DelayedFunction", "resendRequest");
						setValue("Delay", 120.0f); // wait 2 minutes
						mWaitQuota = true;
						mWaitQuotaCount++;
						return nullptr;
					}

					if (code == 32)
					{
						HTTPAsyncRequest* request = (HTTPAsyncRequest*)sender;
						mWaitQuota = true;
						mWaitQuotaCount++;
						Upgrade("DelayedFuncUpgrador");
						setValue("DelayedFunction", "resendRequest");
						setValue("Delay", 120.0f); // wait 2 minutes
						request->ClearHeaders();
						int bearerIndex = request->getValue<int>("BearerIndex");
						// remove bearer from list
						mTwitterBear.erase(mTwitterBear.begin() + bearerIndex);
						mAnswer->AddHeader(mTwitterBear[NextBearer()]);
						mAnswer->setValue("BearerIndex", CurrentBearer());
						return nullptr;
					}

				}
				if (result["errors"][0]["title"])
				{
					// check if data was retrieved
					if (!result["data"] || (result["data"]->size() == 0))
					{
						std::string t(result["errors"][0]["title"]);
						if ((t == "Not Found Error") || (t == "Forbidden") || (t == "Authorization Error"))
						{
							mApiErrorCode = 63;
							return nullptr;
						}
					}
				}
			}

			return result;

		}
	}

	return nullptr;
}

DEFINE_METHOD(TwitterConnect, getUserDetails)
{
	auto json = RetrieveJSON(sender);

	if (json)
	{
		CoreItemSP	data = json["data"];
		CoreItemSP	public_m = data["public_metrics"];

		UserStruct CurrentUserStruct;

		CurrentUserStruct.mID = data["id"];
		CurrentUserStruct.mName = data["username"];
		CurrentUserStruct.mFollowersCount = public_m["followers_count"];
		CurrentUserStruct.mFollowingCount = public_m["following_count"];
		CurrentUserStruct.mStatuses_count = public_m["tweet_count"];
		CurrentUserStruct.UTCTime = data["created_at"]->toString();
		CurrentUserStruct.mThumb.mURL = CleanURL(data["profile_image_url"]);

		EmitSignal("UserDetailRetrieved", CurrentUserStruct);
	}
	else if (!mWaitQuota)
	{

		if (mApiErrorCode == 63) // user suspended
		{
			u64 id = sender->getValue<u64>("UserID");
			UserStruct suspended;
			suspended.mName = usString("Unknown");
			suspended.mID = id;
			suspended.mFollowersCount = 0;
			suspended.mFollowingCount = 0;
			suspended.mStatuses_count = 0;

			EmitSignal("UserDetailRetrieved", suspended);
			
			mApiErrorCode = 0;
		}

	}

	return true;
}



DEFINE_METHOD(TwitterConnect, getTweets)
{
	auto json = RetrieveJSON(sender);

	if (json)
	{

		std::vector<Twts> retrievedTweets;
		CoreItemSP tweetsArray = json["data"];
		if (tweetsArray)
		{

			CoreItemSP includesTweets = json["includes"]["tweets"];

			auto searchRetweeted = [this, includesTweets](u64 twtid)->CoreItemSP
			{
				for (auto t : includesTweets)
				{
					if ((u64)t["id"] == twtid)
					{
						return t;
					}
				}
			};

			unsigned int tweetcount = tweetsArray->size();

			for (unsigned int i = 0; i < tweetcount; i++)
			{
				CoreItemSP currentTweet = tweetsArray[i];
				CoreItemSP RTStatus = currentTweet["referenced_tweets"];

				u32	isReply = 0;

				u64 tweetid = currentTweet["id"];
				
				if (RTStatus)
				{
					u32 referenced_tweets_count = RTStatus->size();

					for (auto r : RTStatus)
					{
						if (r["type"])
						{
							std::string type(r["type"]);
							if (type == "replied_to")
							{
								isReply = 1;
							}
							else if (type == "retweeted")
							{
								// change tweet id and get authorid
								tweetid = r["id"];
								currentTweet = searchRetweeted(tweetid);
								break;
							}
						}
					}
				}
				u64 authorid = currentTweet["author_id"];
				std::string tweetdate = currentTweet["created_at"];

				usString text(currentTweet["text"]);

				SaveTweet(text, authorid, tweetid);

				u32 like_count = currentTweet["public_metrics"]["like_count"];
				u32 rt_count = currentTweet["public_metrics"]["retweet_count"];
				u32 quote_count = currentTweet["public_metrics"]["quote_count"];

				u32		creationDate = GetU32YYYYMMDD(tweetdate).first;
				
				retrievedTweets.push_back({ authorid,tweetid,like_count,rt_count,quote_count,creationDate,isReply });
				
			}
		}

		std::string nextStr = "-1";

		CoreItemSP meta = json["meta"];
		if (meta)
		{
			if (meta["next_token"])
			{
				nextStr = meta["next_token"]->toString();
				if (nextStr == "0")
				{
					nextStr = "-1";
				}
			}
		}

		EmitSignal("TweetRetrieved", retrievedTweets, nextStr);

	}

	return true;
}



DEFINE_METHOD(TwitterConnect, getLikers)
{
	auto json = RetrieveJSON(sender);

	if (json)
	{
		std::vector<u64> retrievedLikers;
		CoreItemSP likersArray = json["data"];
		if (likersArray)
		{
			unsigned int likerscount = likersArray->size();

			for (unsigned int i = 0; i < likerscount; i++)
			{
				CoreItemSP currentLiker = likersArray[i];

				u64 likerid = currentLiker["id"];
				retrievedLikers.push_back(likerid);
			}
		}

		std::string nextStr = "-1";

		CoreItemSP meta = json["meta"];
		if (meta)
		{
			if (meta["next_token"])
			{
				nextStr = meta["next_token"]->toString();
				if (nextStr == "0")
				{
					nextStr = "-1";
				}
			}
		}

		EmitSignal("LikersRetrieved", retrievedLikers, nextStr);

	}
	

	return true;
}




DEFINE_METHOD(TwitterConnect, getFavorites)
{
	auto json = RetrieveJSON(sender);
	std::vector<Twts> currentFavorites;
	std::string nextStr = "-1";

	if (json)
	{
		CoreItemSP tweetsArray = json["data"];

		if (tweetsArray)
		{
			CoreItemSP includesTweets = json["includes"]["tweets"];

			auto searchRetweeted = [this, includesTweets](u64 twtid)->CoreItemSP
			{
				for (auto t : includesTweets)
				{
					if ((u64)t["id"] == twtid)
					{
						return t;
					}
				}
			};

			unsigned int tweetcount = tweetsArray->size();

			for (unsigned int i = 0; i < tweetcount; i++)
			{
				CoreItemSP currentTweet = tweetsArray[i];

				CoreItemSP RTStatus = currentTweet["referenced_tweets"];

				u32	isReply = 0;
				u64 tweetid = currentTweet["id"];

				if (RTStatus)
				{
					u32 referenced_tweets_count = RTStatus->size();

					for (auto r : RTStatus)
					{
						if (r["type"])
						{
							std::string type(r["type"]);
							if (type == "replied_to")
							{
								isReply = 1;
							}
							else if (type == "retweeted")
							{
								// change tweet id and get authorid
								tweetid = r["id"];
								currentTweet = searchRetweeted(tweetid);
								break;
							}
						}
					}
				}


				std::string tweetdate = currentTweet["created_at"];

				u64 authorid = currentTweet["author_id"];

				usString text(currentTweet["text"]);

				SaveTweet(text, authorid, tweetid);

				u32 like_count = currentTweet["public_metrics"]["like_count"];
				u32 rt_count = currentTweet["public_metrics"]["retweet_count"];
				u32 quote_count = currentTweet["public_metrics"]["quote_count"];

				u32		creationDate = GetU32YYYYMMDD(tweetdate).first;

				//if (inGoodInterval(tweetdate, tweetid)) // too problematic to retrieve just the good ones by date for favorites
				{
					currentFavorites.push_back({ authorid,tweetid,like_count,rt_count,quote_count,creationDate,isReply });
				}
			}
		}
		CoreItemSP meta = json["meta"];
		if (meta)
		{
			if (meta["next_token"])
			{
				nextStr = meta["next_token"];
				if (nextStr == "0")
				{
					nextStr = "-1";
				}
			}
		}
	}

	if (!mWaitQuota) // can't access favorite for this user
	{
		EmitSignal("FavoritesRetrieved", currentFavorites, nextStr);
	}

	return true;
}


DEFINE_METHOD(TwitterConnect, getFollow)
{
	auto json = RetrieveJSON(sender);

	std::vector<u64>	follow;
	std::string nextStr = "-1";

	if (json)
	{
		CoreItemSP followArray = json["data"];
		if (followArray)
		{
			unsigned int idcount = followArray->size();
			for (unsigned int i = 0; i < idcount; i++)
			{
				u64 id = followArray[i]["id"];
				follow.push_back(id);
			}
		}

		CoreItemSP meta = json["meta"];
		if (meta)
		{
			if (meta["next_token"])
			{
				nextStr = meta["next_token"]->toString();
				if (nextStr == "0")
				{
					nextStr = "-1";
				}
			}
		}
	}

	if (!mWaitQuota)
	{
		EmitSignal("FollowRetrieved", follow, nextStr);
	}

	return true;
}


void	TwitterConnect::resendRequest()
{
	KigsCore::addAsyncRequest(mAnswer);
	mAnswer->Init();
	mRequestCount++;
	RequestLaunched(60.5); // max request time to be sure
	mWaitQuota = false;
}


void	TwitterConnect::sendRequest()
{
	mAnswer->Init();
	RequestLaunched(mAnswer->getValue<float>("WaitDelay"));
	mRequestCount++;
}

void	TwitterConnect::thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader)
{

	// search used downloader in vector
	std::vector<std::pair<CMSP, std::pair<u64, UserStruct*>> > tmpList;

	for (const auto& p : mDownloaderList)
	{
		if (p.first.get() == downloader)
		{

			std::string	url = downloader->getValue<std::string>("URL");
			std::string::size_type pos = url.rfind('.');
			std::string ext = url.substr(pos);
			SP<TinyImage> img = nullptr;

			if (ext == ".png")
			{
				img = MakeRefCounted<PNGClass>(data);
			}
			else if (ext == ".gif")
			{
				img = MakeRefCounted<GIFClass>(data);
			}
			else
			{
				img = MakeRefCounted<JPEGClass>(data);
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

				SmartPointer<TinyImage>	imgsp = img->shared_from_this();
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
			}
		}
		else
		{
			tmpList.push_back(p);
		}
	}
	mDownloaderList = std::move(tmpList);
}

std::pair<u32,u32>	TwitterConnect::GetU32YYYYMMDD(const std::string& utcdate)
{
	int Year, Month, Day, Hour, Minutes, Seconds;

	sscanf(utcdate.c_str(), "%d-%d-%dT%d:%d:%d.000Z", &Year, &Month, &Day, &Hour, &Minutes, &Seconds);

	return {Year*10000 + Month * 100 + Day,((Hour*60)+Minutes*60)+Seconds};
}


// fromdate & todate are valid dates in YYYY-MM-DD format
void	TwitterConnect::initDates(const std::string& fromdate, const std::string& todate)
{
	mDates[0].dateAsString = fromdate ;
	mDates[0].dateAsInt = GetU32YYYYMMDD(mDates[0].dateAsString + "T0:0:0.000Z").first;
	mDates[1].dateAsString = todate ;
	mDates[1].dateAsInt = GetU32YYYYMMDD(mDates[1].dateAsString + "T23:59:59.000Z").first;
	mUseDates = true;
}

u64	TwitterConnect::getTweetIdBeforeDate(const std::string& date)
{
	auto result = GetU32YYYYMMDD(date + "T0:0:0.000Z");

	u64 foundid = 0;
	for (const auto &d : mDateToTweet)
	{
		if (result.first > d.first)
		{
			foundid = d.second.first;
		}
		else
		{
			return foundid;
		}
	}
	return foundid;
}
u64	TwitterConnect::getTweetIdAfterDate(const std::string& date)
{
	auto result = GetU32YYYYMMDD(date + "T23:59:59.000Z");

	for (const auto& d : mDateToTweet)
	{
		if (result.first < d.first)
		{
			return d.second.first;
		}
	}
	return 0;
}

std::map<std::string, u32>	monthes = { {"Jan",1}, {"Feb",2}, {"Mar",3}, {"Apr",4},
										{"May",5}, {"Jun",6}, {"Jul",7}, {"Aug",8},
										{"Sep",9}, {"Oct",10}, {"Nov",11}, {"Dec",12} };

std::string TwitterConnect::creationDateToUTC(const std::string& created_date)
{
	char Day[6];
	char Mon[6];

	int	day_date;
	int	hours, minutes, seconds;
	int delta;
	int	year = 0;

	sscanf(created_date.c_str(), "%s %s %d %d:%d:%d +%d %d", Day, Mon, &day_date, &hours, &minutes, &seconds, &delta, &year);

	std::string result=std::to_string(year) + "-" + std::to_string(monthes[(std::string)Mon]) + "-" + std::to_string(day_date) +"T"+ std::to_string(hours) + ":" + std::to_string(minutes) + ":" + std::to_string(seconds) + ".000Z";

	return result;
}

bool TwitterConnect::inGoodInterval(const std::string& createdDate, u64 tweetid)
{
	if (!mUseDates)
		return true;

	auto result = GetU32YYYYMMDD(createdDate);

	if ((result.first >= mDates[0].dateAsInt) && (result.first <= mDates[1].dateAsInt))
		return true;

	return false;
}

int	TwitterConnect::getDateDiffInMonth(const std::string& date1, const std::string& date2)
{
	u32 result1 = GetU32YYYYMMDD(date1).first;
	u32 result2 = GetU32YYYYMMDD(date2).first;

	if (result1 > result2)
	{
		std::swap(result1, result2);
	}

	int d1 = result1 % 100;
	result1 -= d1;
	result1 /= 100;
	int d2 = result2 % 100;
	result2 -= d2;
	result2 /= 100;

	int m1 = result1 % 100;
	result1 -= m1;
	result1 /= 100;
	int m2 = result2 % 100;
	result2 -= m2;
	result2 /= 100;

	int dy = result2 - result1;

	int dm = (12 - m1) + m2-1 + ((d2 >= d1) ? 1 : 0);
	dm += (dy - 1) * 12;

	return dm;
}

std::string	TwitterConnect::getTodayDate()
{
	char today[64];

	time_t now = time(NULL);
	struct tm* t = localtime(&now);

	sprintf(today, "%d-%02d-%02dT", 2000 + t->tm_year - 100, t->tm_mon + 1, t->tm_mday);

	return std::string(today)+"T00:00:00.000Z";
}

float	TwitterConnect::PerAccountUserMap::GetNormalisedSimilitude(const PerAccountUserMap& other)
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

float	TwitterConnect::PerAccountUserMap::GetNormalisedAttraction(const PerAccountUserMap& other)
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