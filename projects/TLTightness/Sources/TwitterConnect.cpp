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

CoreItemSP	TwitterConnect::LoadJSon(const std::string& fname, bool utf16)
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

TwitterConnect::TwitterConnect(const kstl::string& name, CLASS_NAME_TREE_ARG) : CoreModifiable(name, PASS_CLASS_NAME_TREE_ARG)
{
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;
	mCurrentTime = time(0);
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

	CoreItemSP initP = LoadJSon(filename, true);

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
	ch.UTCTime = initP["CreatedAt"];
	if (initP["ImgURL"])
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


void TwitterConnect::launchUserDetailRequest(const std::string& UserName, UserStruct& ch, const std::string& signal)
{

	mCurrentUserStruct = &ch;
	mSignal = signal;

	// check classic User Cache
	std::string url = "2/users/by/username/" + UserName + "?user.fields=created_at,public_metrics,profile_image_url";
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
	mAnswer->AddHeader(mTwitterBear[NextBearer()]);
	mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
	mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", true);
	mAnswer->AddDynamicAttribute<maFloat, float>("WaitDelay", 1.1);

	double waitdelay = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime;
	mNextRequestDelay -= waitdelay;

	if (mNextRequestDelay < 0.0)
	{
		mAnswer->Init();
		mRequestCount++;
		RequestLaunched(1.1);
	}
	else
	{
		Upgrade("DelayedFuncUpgrador");
		setValue("DelayedFunction", "sendRequest");
		setValue("Delay", mNextRequestDelay);
	}

}

void TwitterConnect::launchUserDetailRequest(u64 UserID, UserStruct& ch, const std::string& signal)
{
	mCurrentUserStruct = &ch;
	mSignal = signal;

	// check classic User Cache
	std::string url = "2/users/" + std::to_string(UserID) + "?user.fields=created_at,public_metrics,profile_image_url";
	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getUserDetails", this);
	mAnswer->AddHeader(mTwitterBear[NextBearer()]);
	mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
	mAnswer->AddDynamicAttribute<maBool, bool>("RequestThumb", true);
	mAnswer->AddDynamicAttribute<maFloat, float>("WaitDelay", 1.1);

	double waitdelay = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime;
	mNextRequestDelay -= waitdelay;

	if (mNextRequestDelay < 0.0)
	{
		mAnswer->Init();
		mRequestCount++;
		RequestLaunched(1.1);
	}
	else
	{
		Upgrade("DelayedFuncUpgrador");
		setValue("DelayedFunction", "sendRequest");
		setValue("Delay", mNextRequestDelay);
	}
}



void	TwitterConnect::launchGetFollowing(UserStruct& ch, const std::string& signal)
{
	mCurrentUserStruct = &ch;
	mSignal = signal;

	if (mNextCursor == "-1")
	{
		mCurrentIDVector.clear();
	}
	std::string url = "1.1/friends/ids.json?";
	if (mNextCursor != "-1")
	{
		url += "cursor=" + mNextCursor + "&";
	}
	url += "user_id=" + GetIDString(ch.mID) + "&count=5000";

	mAnswer = mTwitterConnect->retreiveGetAsyncRequest(url.c_str(), "getFollowing", this);
	mAnswer->AddDynamicAttribute<maULong, u64>("UserID", ch.mID);
	mAnswer->AddHeader(mTwitterBear[NextBearer()]);
	mAnswer->AddDynamicAttribute<maInt, int>("BearerIndex", CurrentBearer());
	mAnswer->AddDynamicAttribute<maFloat, float>("WaitDelay", 60.5f);

	double waitdelay = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime;
	mNextRequestDelay -= waitdelay;

	if (mNextRequestDelay<0.0)
	{	
		mAnswer->Init();
		mRequestCount++;
		RequestLaunched(60.5);
	}
	else
	{
		Upgrade("DelayedFuncUpgrador");
		setValue("DelayedFunction", "sendRequest");
		setValue("Delay", mNextRequestDelay); 
	}
}

std::vector<u64>		TwitterConnect::LoadIDVectorFile(const std::string& filename,bool &fileExist)
{
	fileExist = false;
	std::vector<u64> loaded;
	if (LoadDataFile<u64>(filename, loaded))
	{
		fileExist = true;
	}

	return loaded;
}

void		TwitterConnect::SaveFollowingFile(u64 id, const std::vector<u64>& v)
{
	std::string filename = "Cache/Users/" + GetUserFolderFromID(id) + "/" + GetIDString(id) + ".ids";
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

		}
		else
		{

			usString	utf8string((UTF8Char*)validstring.c_str());

			JSonFileParserUTF16 L_JsonParser;
			CoreItemSP result = L_JsonParser.Get_JsonDictionaryFromString(utf8string);

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
						
					}

					if (code == 32)
					{
						HTTPAsyncRequest* request = (HTTPAsyncRequest*)sender;
						mWaitQuota = true;
						Upgrade("DelayedFuncUpgrador");
						setValue("DelayedFunction", "resendRequest");
						setValue("Delay", 120.0f); // wait 2 minutes
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

DEFINE_METHOD(TwitterConnect, getUserDetails)
{
	auto json = RetrieveJSON(sender);

	if (json)
	{
		CoreItemSP	data = json["data"];
		CoreItemSP	public_m = data["public_metrics"];

		mCurrentUserStruct->mID = data["id"];
		mCurrentUserStruct->mName = data["username"];
		mCurrentUserStruct->mFollowersCount = public_m["followers_count"];
		mCurrentUserStruct->mFollowingCount = public_m["following_count"];
		mCurrentUserStruct->mStatuses_count = public_m["tweet_count"];
		mCurrentUserStruct->UTCTime = data["created_at"];
		mCurrentUserStruct->mThumb.mURL = CleanURL(data["profile_image_url"]);

		SaveUserStruct(mCurrentUserStruct->mID, *mCurrentUserStruct);
		bool requestThumb;
		if (sender->getValue("RequestThumb", requestThumb))
		{
			if (requestThumb)
				if (!LoadThumbnail(mCurrentUserStruct->mID, *mCurrentUserStruct))
				{
					LaunchDownloader(mCurrentUserStruct->mID, *mCurrentUserStruct);
				}
		}
		
		EmitSignal(mSignal);
		mAnswer = nullptr;
	}
	else if (!mWaitQuota)
	{

		if (mApiErrorCode == 63) // user suspended
		{
			u64 id = sender->getValue<u64>("UserID");
			UserStruct suspended;
			suspended.mFollowersCount = 0;
			suspended.mFollowingCount = 0;
			suspended.mStatuses_count = 0;
			SaveUserStruct(id, suspended);
			mApiErrorCode = 0;
		}

	}

	return true;
}

DEFINE_METHOD(TwitterConnect, getFollowing)
{
	auto json = RetrieveJSON(sender);

	if (json)
	{

		mCurrentUserStruct->mID = sender->getValue<u64>("UserID");

		CoreItemSP followingArray = json["ids"];
		unsigned int idcount = followingArray->size();
		for (unsigned int i = 0; i < idcount; i++)
		{
			u64 id = followingArray[i];
			mCurrentIDVector.push_back(id);
		}

		mNextCursor = json["next_cursor_str"];
		if (mNextCursor == "0")
		{
			mNextCursor = "-1";
		}

		if (mNextCursor == "-1")
		{
			SaveFollowingFile(mCurrentUserStruct->mID, mCurrentIDVector);
			mCurrentUserStruct->mFollowing = mCurrentIDVector;
			EmitSignal(mSignal);
			mAnswer = nullptr;
		}
		else
		{
			// do it again
			launchGetFollowing(*mCurrentUserStruct,mSignal);
		}

	}
	else
	{
		if (!mWaitQuota)
		{
			// this user is not available, go to next one
			
			SaveFollowingFile(mCurrentUserStruct->mID, mCurrentIDVector);
			EmitSignal(mSignal);
			mAnswer = nullptr;
			
		}
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