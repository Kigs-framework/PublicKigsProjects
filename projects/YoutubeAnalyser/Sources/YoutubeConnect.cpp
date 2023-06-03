#include "YoutubeConnect.h"
#include "JSonFileParser.h"
#include "HTTPRequestModule.h"
#include "TextureFileManager.h"
#include "TinyImage.h"
#include "JPEGClass.h"
#include "PNGClass.h"
#include "GIFClass.h"

using namespace Kigs;
using namespace Kigs::File;

double		YoutubeConnect::mOldFileLimit = 0;
time_t		YoutubeConnect::mCurrentTime = 0;

YoutubeConnect::datestruct	YoutubeConnect::mDates[2];
bool						YoutubeConnect::mUseDates = false;

YoutubeConnect* YoutubeConnect::mInstance = nullptr;

IMPLEMENT_CLASS_INFO(YoutubeConnect)

void	YoutubeConnect::initBearer(CoreItemSP initP)
{
	// retreive parameters
	CoreItemSP foundBear;
	int bearIndex = 0;
	do
	{
		char	BearName[64];
		++bearIndex;
		sprintf(BearName, "GoogleKey%d", bearIndex);

		foundBear = initP[(std::string)BearName];

		if (foundBear)
		{
			mYoutubeBear.push_back("&key=" + (std::string)foundBear);
		}
	} while (foundBear);
}



bool YoutubeConnect::checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle, double OldFileLimit)
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

void		YoutubeConnect::SaveJSon(const std::string& fname, const CoreItemSP& json, bool utf16)
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


CoreItemSP	YoutubeConnect::LoadJSon(const std::string& fname, bool useOldFileLimit, bool utf16)
{
	SmartPointer<::FileHandle> filenamehandle;

	CoreItemSP initP(nullptr);
	if (checkValidFile(fname, filenamehandle, useOldFileLimit ? mOldFileLimit : 0.0))
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

YoutubeConnect::YoutubeConnect(const std::string& name, CLASS_NAME_TREE_ARG) : CoreModifiable(name, PASS_CLASS_NAME_TREE_ARG)
{
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;
	mCurrentTime = time(0);

	YoutubeConnect::mInstance = this;

	mDates[0].dateAsInt = mDates[1].dateAsInt = 0;
	mDates[0].dateAsString = mDates[1].dateAsString = "";

}

void	YoutubeConnect::initConnection(double oldfiletime)
{
	mOldFileLimit = oldfiletime;

	// init twitter connection
	mYoutubeConnect = KigsCore::GetInstanceOf("googleConnect", "HTTPConnect");
	mYoutubeConnect->setValue("HostName", "www.googleapis.com");
	mYoutubeConnect->setValue("Type", "HTTPS");
	mYoutubeConnect->setValue("Port", "443");
	mYoutubeConnect->Init();
}

bool			YoutubeConnect::LoadChannelID(const std::string& channelName, std::string& channelID)
{
	std::string filename = "Cache/" + channelName + "/" + channelName + ".json";
	CoreItemSP currentP = LoadJSon(filename,true);

	if (!currentP) // new channel, launch getChannelID
	{
		return false;
	}

	channelID=currentP["channelID"];

	return true;
}
void			YoutubeConnect::SaveChannelID(const std::string& channelName, const std::string& channelID)
{
	std::string filename = "Cache/" + channelName + "/" + channelName + ".json";

	JSonFileParser L_JsonParser;
	CoreItemSP initP = MakeCoreMap();
	initP->set("channelID", channelID);

	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
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


void		YoutubeConnect::LaunchDownloader(ChannelStruct& ch)
{
	// first, check that downloader for the same thumb is not already launched

	for (const auto& d : mInstance->mDownloaderList)
	{
		if (d.second.first == ch.mID) // downloader found
		{
			return;
		}
	}

	if (ch.mThumb.mURL == "") // need a valid url
	{
		return;
	}

	CMSP CurrentDownloader = KigsCore::GetInstanceOf("downloader", "ResourceDownloader");
	CurrentDownloader->setValue("URL", ch.mThumb.mURL);
	KigsCore::Connect(CurrentDownloader.get(), "onDownloadDone", mInstance, "thumbnailReceived");

	mInstance->mDownloaderList.push_back({ CurrentDownloader , {ch.mID,&ch} });

	CurrentDownloader->Init();
}


std::string  YoutubeConnect::CleanURL(const std::string& url)
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

CoreItemSP		YoutubeConnect::LoadVideoList(const std::string& channelID)
{
	std::string filename = "Cache/" + channelID + "/videos/";
	filename += channelID;

	// if date use them
	if (mUseDates)
	{
		filename += "_from_" + mDates[0].dateAsString + "_";
		filename += "_to_" + mDates[1].dateAsString + "_";
	}

	filename += ".json";

	CoreItemSP initP = LoadJSon(filename,true,true);
	return initP;
}

void				YoutubeConnect::SaveVideoList(const std::string& channelID, const std::vector<videoStruct>& videoList, const std::string& nextPage)
{
	std::string filename = "Cache/" + channelID + "/videos/";
	filename += channelID;

	// if date use them
	if (mUseDates)
	{
		filename += "_from_" + mDates[0].dateAsString + "_";
		filename += "_to_" + mDates[1].dateAsString + "_";
	}

	filename += ".json";

	CoreItemSP initP = LoadJSon(filename);

	if (!initP)
	{
		initP = MakeCoreMap();
	}

	if (nextPage != "")
	{
		initP->set("nextPageToken", nextPage);
	}
	else
	{
		initP->erase("nextPageToken");
	}

	CoreItemSP v = MakeCoreVector();
	// titles
	CoreItemSP nv = MakeCoreVector();

	for (const auto& vid : videoList)
	{
		v->set("", vid.mID);
		nv->set("", vid.mName);
	}

	initP->set("videos", v);
	initP->set("videosTitles", nv);
	JSonFileParserUTF16 L_JsonParser;
	L_JsonParser.Export((CoreMap<usString>*)initP.get(), filename);
}

CoreItemSP		YoutubeConnect::LoadCommentList(const std::string& channelID, const std::string& videoID)
{
	std::string filename = "Cache/" + channelID + "/videos/";
	filename += videoID + "_videos.json";

	CoreItemSP initP = LoadJSon(filename, true, false);
	return initP;
}

void				YoutubeConnect::SaveCommentList(const std::string& channelID, const std::string& videoID, const std::vector<std::string>& authorList, const std::string& nextPage)
{
	std::string filename = "Cache/" + channelID + "/videos/";
	filename += videoID + "_videos.json";

	// start a new file (don't happened)
	CoreItemSP initP = MakeCoreMap();

	if (nextPage != "")
	{
		initP->set("nextPageToken", nextPage);
	}
	
	CoreItemSP v = MakeCoreVector();

	// add authors
	for (const auto& auth : authorList)
	{
		v->set("", auth);
	}

	initP->set("authors", v);

	JSonFileParser L_JsonParser;
	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
}

CoreItemSP		YoutubeConnect::LoadAuthorFile(const std::string& authorID)
{
	std::string filename = "Cache/Authors/";
	filename += authorID + ".json";
	CoreItemSP initP = LoadJSon(filename, true, false);
	return initP;
}
void			YoutubeConnect::SaveAuthorFile(const std::string& authorID, const std::vector<std::string>& subscrList, const std::string& nextPage)
{
	std::string filename = "Cache/Authors/";
	filename += authorID + ".json";

	// start a new file (don't happened)
	CoreItemSP initP = MakeCoreMap();

	if (nextPage != "")
	{
		initP->set("nextPageToken", nextPage);
	}

	CoreItemSP v = MakeCoreVector();

	if (subscrList.size())
	{
		// add channels
		for (const auto& channel : subscrList)
		{
			v->set("", channel);
		}

		initP->set("channels", v);
	}

	JSonFileParser L_JsonParser;
	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
}

void		YoutubeConnect::SaveChannelStruct(const ChannelStruct& ch)
{
	JSonFileParserUTF16 L_JsonParser;
	CoreItemSP initP = MakeCoreMapUS();
	initP->set("Name", ch.mName);
	initP->set("ID", ch.mID);
	initP->set("CreationDate", ch.mCreationDate);
	initP->set("TotalSubscribers", (int)ch.mTotalSubscribers);
	initP->set("ViewCount", (int)ch.mViewCount);
	initP->set("VideoCount", (int)ch.mVideoCount);

	if (ch.mThumb.mURL != "")
	{
		initP->set("ImgURL", (usString)ch.mThumb.mURL);
	}

	std::string folderName = ch.mID.substr(0, 4);
	str_toupper(folderName);

	std::string filename = "Cache/Channels/";
	filename += folderName + "/" + ch.mID + ".json";

	L_JsonParser.Export((CoreMap<usString>*)initP.get(), filename);
}



// load json channel file dans fill YoutubeConnect::ChannelStruct
bool		YoutubeConnect::LoadChannelStruct(const std::string& id, ChannelStruct& ch, bool requestThumb)
{
	std::string filename = "Cache/Channels/";

	std::string folderName = id.substr(0, 4);
	str_toupper(folderName);

	filename += folderName + "/" + id + ".json";

	CoreItemSP initP = LoadJSon(filename, true,true);

	if (!initP) // file not found, return
	{
		return false;
	}

	ch.mID = id;
	ch.mName = (usString)initP["Name"];
	ch.mTotalSubscribers = initP["TotalSubscribers"];
	ch.mCreationDate = initP["CreationDate"];
	ch.mViewCount = initP["ViewCount"];
	ch.mVideoCount = initP["VideoCount"];
	if (initP["ImgURL"])
	{
		ch.mThumb.mURL = initP["ImgURL"];
	}
	if (requestThumb && !ch.mThumb.mTexture)
	{
		filename = "Cache/Thumbs/";
		filename += id;
		filename += ".jpg";

		auto pathManager = KigsCore::Singleton<FilePathManager>();
		SmartPointer<::FileHandle> fullfilenamehandle = pathManager->FindFullName(filename);

		if (fullfilenamehandle->mStatus & FileHandle::Exist)
		{
			auto textureManager = KigsCore::Singleton<Draw::TextureFileManager>();
			ch.mThumb.mTexture = textureManager->GetTexture(filename);
		}
		else
		{
			if (ch.mThumb.mURL != "")
			{
				LaunchDownloader(ch);
			}
		}
	}
	return true;
}


void	YoutubeConnect::launchGetChannelID(const std::string& channelName)
{
	std::string url = "/youtube/v3/channels?part=snippet,statistics&id=";
	std::string request = url + channelName + getCurrentBearer();
	mAnswer = mYoutubeConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelID", this);
	mAnswer->Init();
	mRequestCount++;
}

void	YoutubeConnect::launchGetChannelStats(const std::string& channelID)
{
	// request details
	std::string url = "/youtube/v3/channels?part=statistics,snippet&id=";
	std::string request = url + channelID + getCurrentBearer();
	mAnswer = mYoutubeConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
	mAnswer->Init();
	mRequestCount++;
}

void	YoutubeConnect::launchGetVideo(const std::string& channelID, const std::string& nextPage)
{
	// get latest video list
	// GET https://www.googleapis.com/youtube/v3/search?part=snippet&channelId={CHANNEL_ID}&maxResults=10&order=date&type=video&key={YOUR_API_KEY}
	// https://www.googleapis.com/youtube/v3/search?part=snippet&maxResults=25&q=surfing&key={YOUR_API_KEY}

	std::string url = "/youtube/v3/search?part=snippet";

	url += "&channelId=" + channelID;
	
	std::string request = url + getCurrentBearer() + nextPage + "&maxResults=50&order=date&type=video";

	if (mUseDates)
	{
		request += "&publishedAfter=" + mDates[0].dateAsString + "T00:00:00Z";
		request += "&publishedBefore=" + mDates[1].dateAsString + "T23:59:00Z";
	}

	mAnswer = mYoutubeConnect->retreiveGetAsyncRequest(request.c_str(), "getVideoList", this);
	mAnswer->Init();
	mRequestCount++;
}

void	YoutubeConnect::launchGetComments(const std::string& channelID, const std::string& videoID, const std::string& nextPage)
{
	std::string url = "/youtube/v3/commentThreads?part=snippet%2Creplies&videoId=";
	std::string request = url + videoID + getCurrentBearer() + nextPage + "&maxResults=50&order=time";
	mAnswer = mYoutubeConnect->retreiveGetAsyncRequest(request.c_str(), "getCommentList", this);
	mAnswer->Init();
	mRequestCount++;
}

void	YoutubeConnect::launchGetSubscriptions(const std::string& authorID, const std::string& nextPage)
{
	std::string url = "/youtube/v3/subscriptions?part=snippet%2CcontentDetails&channelId=";
	std::string request = url + authorID + getCurrentBearer() + "&maxResults=50";
	if (nextPage != "")
	{
		request+="&pageToken=" + nextPage;
	}
	mAnswer = mYoutubeConnect->retreiveGetAsyncRequest(request.c_str(), "getUserSubscription", this);
	mAnswer->Init();
	mRequestCount++;
}

CoreItemSP	YoutubeConnect::RetrieveJSON(CoreModifiable* sender)
{
	int errorState = 0;
	void* resultbuffer = nullptr;
	sender->getValue("ReceivedBuffer", resultbuffer);

	if (resultbuffer)
	{
		CoreRawBuffer* r = (CoreRawBuffer*)resultbuffer;
		std::string_view received(r->data(), r->size());

		std::string validstring(received);

		if (validstring.length() == 0)
		{
			//mState = 10;
			//mErrorCode = 405;
		}
		else
		{

			usString	utf8string((UTF8Char*)validstring.c_str());

			JSonFileParserUTF16 L_JsonParser;
			CoreItemSP result = L_JsonParser.Get_JsonDictionaryFromString(utf8string);

			if (result["error"])
			{
				std::string reason = result["error"]["errors"][0]["reason"];
				int code = result["error"]["code"];
				if (code == 403)
				{
					if ((reason == "quotaExceeded") || (reason == "dailyLimitExceeded"))
					{
						errorState = 10;
					}
				}
			}

			if (errorState != 10)
			{
				return result;
			}

		}
	}

	return nullptr;
}

DEFINE_METHOD(YoutubeConnect, getChannelID)
{
	auto json = RetrieveJSON(sender);
	std::string channelID;
	if (json)
	{
		CoreItemSP IDItem = json["items"][0]["id"];

		if (IDItem)
		{
			channelID = IDItem;
		}
	}
	// TODO manage error
	EmitSignal("ChannelIDRetrieved", channelID);
	return true;
}

DEFINE_METHOD(YoutubeConnect, getChannelStats)
{
	auto json = RetrieveJSON(sender);

	ChannelStruct newChannel;
	newChannel.mID = "";

	if (json)
	{
		if (json["items"][0]["id"])
		{
			newChannel.mID = json["items"][0]["id"];

			CoreItemSP infos = json["items"][0]["snippet"];
			if (infos)
			{
				newChannel.mName = (usString)infos["title"];
				newChannel.mThumb.mURL = infos["thumbnails"]["default"]["url"];
				newChannel.mCreationDate = infos["publishedAt"];
			}
			newChannel.mTotalSubscribers = 0;
			newChannel.mViewCount = 0;
			newChannel.mVideoCount = 0;
			CoreItemSP subscount = json["items"][0]["statistics"]["subscriberCount"];
			if (subscount)
			{
				newChannel.mTotalSubscribers = subscount;
			}
			CoreItemSP viewcount = json["items"][0]["statistics"]["viewCount"];
			if (viewcount)
			{
				newChannel.mViewCount = viewcount;
			}
			CoreItemSP vidcount = json["items"][0]["statistics"]["videoCount"];
			if (vidcount)
			{
				newChannel.mVideoCount = vidcount;
			}
		}
	}

	EmitSignal("ChannelStatsRetrieved", newChannel);

	return true;
}

DEFINE_METHOD(YoutubeConnect, getVideoList)
{
	
	auto json = RetrieveJSON(sender);


	std::vector<videoStruct> videoVector;

	std::string nextPageToken;

	if (json)
	{
		if (json["nextPageToken"])
		{
			nextPageToken = json["nextPageToken"];
		}

		CoreItemSP videoList = json["items"];

		if (videoList)
		{
			if (videoList->size())
			{
				for (int i = 0; i < videoList->size(); i++)
				{
					CoreItemSP video = videoList[i];
					if (video["id"]["kind"])
					{
						std::string kind = video["id"]["kind"];
						if (kind == "youtube#video")
						{
							if (video["id"]["videoId"])
							{

								std::string cid = video["snippet"]["channelId"];

								videoStruct toAdd;
								toAdd.mChannelID = cid;
								toAdd.mID = video["id"]["videoId"];
								toAdd.mName = (usString)video["snippet"]["title"];

								videoVector.push_back(toAdd);
							}
						}
					}
				}
			}
		}
	}

	EmitSignal("videoRetrieved", videoVector, nextPageToken);

	return true;
}

DEFINE_METHOD(YoutubeConnect, getCommentList)
{
	auto json = RetrieveJSON(sender);
	std::string nextPageToken;

	std::set<std::string>	authorList;

	if (json)
	{
		if (json["nextPageToken"])
		{
			nextPageToken = json["nextPageToken"];
		}
		CoreItemSP commentThreads = json["items"];

		if (commentThreads)
		{
			if (commentThreads->size())
			{
				for (int i = 0; i < commentThreads->size(); i++)
				{
					CoreItemSP topComment = commentThreads[i]["snippet"]["topLevelComment"]["snippet"];
					if (topComment)
					{

						CoreItemSP authorChannelID = topComment["authorChannelId"]["value"];
						if (authorChannelID)
						{
							std::string authorID = authorChannelID;
							authorList.insert(authorID);
						}
					}
					int answerCount = commentThreads[i]["snippet"]["totalReplyCount"];
					if (answerCount)
					{
						for (int j = 0; j < answerCount; j++)
						{
							CoreItemSP replies = commentThreads[i]["replies"]["comments"][j]["snippet"];
							if (replies)
							{
								CoreItemSP authorChannelID = replies["authorChannelId"]["value"];
								if (authorChannelID)
								{
									std::string authorID = authorChannelID;
									authorList.insert(authorID);
								}
							}
						}
					}
				}
			}
		}
	}

	EmitSignal("commentRetrieved", authorList, nextPageToken);

	return true;
}

DEFINE_METHOD(YoutubeConnect, getUserSubscription)
{	
	std::string nextPageToken;

	std::vector<std::string>	subscripList;

	auto json = RetrieveJSON(sender);

	if (json)
	{
		if (json["nextPageToken"])
		{
			nextPageToken = json["nextPageToken"];
		}

		CoreItemSP subscriptions = json["items"];
		if (subscriptions)
		{
			if (subscriptions->size())
			{
				for (int i = 0; i < subscriptions->size(); i++)
				{
					CoreItemSP subscription = subscriptions[i]["snippet"]["resourceId"]["channelId"];
					if (subscription)
					{
						
						std::string chanID = subscription;
						subscripList.push_back(chanID);
						
					}
				}
			}
		}
	}


	EmitSignal("subscriptionRetrieved", subscripList, nextPageToken);


	return true;
}

void	YoutubeConnect::thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader)
{

	// search used downloader in vector
	std::vector<std::pair<CMSP, std::pair<std::string, YoutubeConnect::ChannelStruct*>> > tmpList;

	for (const auto& p : mDownloaderList)
	{
		if (p.first.get() == downloader)
		{
			SP<Pict::TinyImage> img = MakeRefCounted<Pict::JPEGClass>(data);
			if (img)
			{
				YoutubeConnect::ChannelStruct* toFill = p.second.second;

				std::string filename = "Cache/Thumbs/";
				filename += p.second.first;
				filename += ".jpg";

				// export image
				SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
				if (L_File->mFile)
				{
					Platform_fwrite(data->buffer(), 1, data->length(), L_File.get());
					Platform_fclose(L_File.get());
				}

				toFill->mThumb.mTexture = KigsCore::GetInstanceOf((std::string)toFill->mName + "tex", "Texture");
				toFill->mThumb.mTexture->Init();

				toFill->mThumb.mTexture->CreateFromImage(img);
			}
		}
		else
		{
			tmpList.push_back(p);
		}
	}
	mDownloaderList = std::move(tmpList);
}


std::pair<u32, u32>	YoutubeConnect::GetU32YYYYMMDD(const std::string& utcdate)
{
	int Year, Month, Day, Hour, Minutes, Seconds;

	sscanf(utcdate.c_str(), "%d-%d-%dT%d:%d:%d.000Z", &Year, &Month, &Day, &Hour, &Minutes, &Seconds);

	return { Year * 10000 + Month * 100 + Day,((Hour * 60) + Minutes * 60) + Seconds };
}


// fromdate & todate are valid dates in YYYY-MM-DD format
void	YoutubeConnect::initDates(const std::string& fromdate, const std::string& todate)
{
	mDates[0].dateAsString = fromdate;
	mDates[0].dateAsInt = GetU32YYYYMMDD(mDates[0].dateAsString + "T0:0:0.000Z").first;
	mDates[1].dateAsString = todate;
	mDates[1].dateAsInt = GetU32YYYYMMDD(mDates[1].dateAsString + "T23:59:59.000Z").first;
	mUseDates = true;
}


std::map<std::string, u32>	monthes = { {"Jan",1}, {"Feb",2}, {"Mar",3}, {"Apr",4},
										{"May",5}, {"Jun",6}, {"Jul",7}, {"Aug",8},
										{"Sep",9}, {"Oct",10}, {"Nov",11}, {"Dec",12} };

std::string YoutubeConnect::creationDateToUTC(const std::string& created_date)
{
	char Day[6];
	char Mon[6];

	int	day_date;
	int	hours, minutes, seconds;
	int delta;
	int	year = 0;

	sscanf(created_date.c_str(), "%s %s %d %d:%d:%d +%d %d", Day, Mon, &day_date, &hours, &minutes, &seconds, &delta, &year);

	std::string result = std::to_string(year) + "-" + std::to_string(monthes[(std::string)Mon]) + "-" + std::to_string(day_date) + "T" + std::to_string(hours) + ":" + std::to_string(minutes) + ":" + std::to_string(seconds) + ".000Z";

	return result;
}
/*
time_t	YoutubeConnect::Twts::getCreationDate() const
{

	u32 treatedDate = mCreationDate;
	u32 day = mCreationDate % 100;
	treatedDate -= day;
	treatedDate /= 100;
	u32 month = treatedDate % 100;
	treatedDate -= month;
	treatedDate /= 100;
	u32 year = treatedDate;

	std::string strdate = std::to_string(year) + ":" + std::to_string(month) + ":" + std::to_string(day);

	struct std::tm tm;
	memset(&tm, 0, sizeof(std::tm));
	std::istringstream ss(strdate);
	ss >> std::get_time(&tm, "%Y:%m:%d");
	std::time_t creationtime = mktime(&tm);

	return creationtime;
}*/


int	YoutubeConnect::getDateDiffInMonth(const std::string& date1, const std::string& date2)
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

	int dm = (12 - m1) + m2 - 1 + ((d2 >= d1) ? 1 : 0);
	dm += (dy - 1) * 12;

	return dm;
}

std::string	YoutubeConnect::getTodayDate()
{
	char today[64];

	time_t now = time(NULL);
	struct tm* t = localtime(&now);

	sprintf(today, "%d-%02d-%02dT", 2000 + t->tm_year - 100, t->tm_mon + 1, t->tm_mday);

	return std::string(today) + "T00:00:00.000Z";
}

float	YoutubeConnect::PerAccountUserMap::GetNormalisedSimilitude(const PerAccountUserMap& other)
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

float	YoutubeConnect::PerAccountUserMap::GetNormalisedAttraction(const PerAccountUserMap& other)
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