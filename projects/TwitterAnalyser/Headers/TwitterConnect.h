#pragma once
#include "Texture.h"
#include "HTTPConnect.h"
#include "CoreBaseApplication.h"

class TwitterConnect : public CoreModifiable
{
protected:
	// bearer mamangement
	std::vector<std::string>	mTwitterBear;
	unsigned int				mCurrentBearer = 0;
	
	double		mNextRequestDelay = 0.0;
	double		mLastRequestTime = 0.0;

	static double		mOldFileLimit;

	// get current time once at launch to compare with modified file date
	static time_t     mCurrentTime;
	unsigned int		mRequestCount = 0;

public:

	DECLARE_CLASS_INFO(TwitterConnect, CoreModifiable, TwitterConnect);
	DECLARE_CONSTRUCTOR(TwitterConnect);

	class ThumbnailStruct
	{
	public:
		SP<Texture>					mTexture = nullptr;
		std::string					mURL = "";
	};

	struct Twts
	{
		u64		mAuthorID;		// if 0 need to search author using tweetid
		u64		mTweetID;
		u64		mConversationID;
		u32		mLikeCount;
		u32		mRetweetCount; 
		u32		mQuoteCount;
		u32		mCreationDate;
		u64		mReplyedTo;			// 0 if not a reply, else author id of the replied tweet

		time_t	getCreationDate() const;
	};

	class UserStruct
	{
	public:
		usString					mName = std::string("");
		unsigned int				mFollowersCount = 0;
		unsigned int				mFollowingCount = 0;
		unsigned int				mStatuses_count = 0;
		std::string					UTCTime = "";
		ThumbnailStruct				mThumb;
		u64							mID;
	};
	// structures
	class PerAccountUserMap
	{
	public:
		PerAccountUserMap() :m(nullptr), mSubscribedCount(0), mSize(0)
		{

		}

		PerAccountUserMap(int SSize)
		{
			mSize = SSize;
			m = new unsigned char[mSize];
			memset(m, 0, mSize * sizeof(unsigned char));
		}
		PerAccountUserMap(const PerAccountUserMap& other)
		{
			if (m)
			{
				delete[] m;
			}
			mSize = other.mSize;
			m = new unsigned char[mSize];
			memcpy(m, other.m, mSize * sizeof(unsigned char));
			mSubscribedCount = other.mSubscribedCount;
		}

		PerAccountUserMap& operator=(const PerAccountUserMap& other)
		{
			if (m)
			{
				delete[] m;
			}
			mSize = other.mSize;
			m = new unsigned char[mSize];
			memcpy(m, other.m, mSize * sizeof(unsigned char));
			mSubscribedCount = other.mSubscribedCount;
			mThumbnail = other.mThumbnail;
			return *this;
		}

		~PerAccountUserMap()
		{
			delete[] m;
		}

		void	SetSubscriber(int index)
		{
			m[index] = 1;
			mSubscribedCount++;
		}

		unsigned char* m = nullptr;
		std::vector<std::pair<int, float>>	mCoeffs;
		unsigned int		mSubscribedCount = 0;
		unsigned int		mSize = 0;

		CMSP				mThumbnail;
		v2f					mForce;
		v2f					mPos;
		float				mRadius;

		float	GetNormalisedSimilitude(const PerAccountUserMap& other);
		float	GetNormalisedAttraction(const PerAccountUserMap& other);
	};



	static bool searchDuplicateTweet(u64 twtid,const std::vector< Twts>& tweets) // return true if the same tweet already in the list
	{
		for (const auto& t : tweets)
		{
			if (t.mTweetID == twtid)
			{
				return true;
			}
		}
		return false;
	}

	unsigned int				NextBearer()
	{
		mCurrentBearer = (mCurrentBearer + (unsigned int)1) % mTwitterBear.size();
		return mCurrentBearer;
	}
	unsigned int				CurrentBearer()
	{
		return mCurrentBearer;
	}

	// HTTP Request management
	SP<HTTPConnect>									mTwitterConnect = nullptr;
	SP<HTTPAsyncRequest>							mAnswer = nullptr;

	static CoreItemSP	LoadJSon(const std::string& fname, bool useOldFileLimit=true,bool utf16 = false);
	static void		SaveJSon(const std::string& fname, const CoreItemSP& json, bool utf16=false);

	static bool		checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle, double OldFileLimit);

	// manage wait time between requests
	void	RequestLaunched(double toWait)
	{
		mNextRequestDelay = toWait / mTwitterBear.size();
		mLastRequestTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
	}

	bool	CanLaunchRequest()
	{
		double dt = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime;
		if (dt > mNextRequestDelay)
		{
			return true;
		}
		return false;
	}

	float	getDelay()
	{
		double dt = mNextRequestDelay-(KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime);
		return dt;
	}

	void	initBearer(CoreItemSP f);
	void	initConnection(double oldfiletime);


	static void		LaunchDownloader(u64 id, UserStruct& ch);
	static bool		LoadUserStruct(u64 id, UserStruct& ch, bool requestThumb);
	static void		SaveUserStruct(u64 id, UserStruct& ch);
	static std::string	GetUserFolderFromID(u64 id);
	static std::string	GetIDString(u64 id);
	static std::string  CleanURL(const std::string& url);
	static std::vector<u64>		LoadIDVectorFile(const std::string& filename, bool& fileExist,bool oldfilelimit=true);
	static void					SaveFollowFile(u64 id, const std::vector<u64>& v,const std::string& followtype);
	static bool					LoadFollowFile(u64 id, std::vector<u64>& follow, const std::string& followtype);
	static void					SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename);

	template<typename T>
	static bool	LoadDataFile(const std::string& filename, std::vector<T>& loaded, bool useOldFileLimit = true)
	{
		SmartPointer<::FileHandle> L_File;

		if (checkValidFile(filename, L_File, useOldFileLimit?mOldFileLimit:0.0))
		{
			if (Platform_fopen(L_File.get(), "rb"))
			{
				// get file size
				Platform_fseek(L_File.get(), 0, SEEK_END);
				long filesize = Platform_ftell(L_File.get());
				Platform_fseek(L_File.get(), 0, SEEK_SET);

				loaded.resize(filesize / sizeof(T));

				Platform_fread(loaded.data(), sizeof(T), loaded.size(), L_File.get());
				Platform_fclose(L_File.get());
				return true;
			}
		}


		return false;
	}

	template<typename T>
	static void	SaveDataFile(const std::string& filename, const std::vector<T>& saved)
	{
		SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
		if (L_File->mFile)
		{
			Platform_fwrite(saved.data(), 1, saved.size() * sizeof(T), L_File.get());
			Platform_fclose(L_File.get());
		}
	}

	static bool	LoadThumbnail(u64 id, UserStruct& ch);


	void	launchUserDetailRequest(const std::string& UserName, UserStruct& ch);
	void	launchUserDetailRequest(u64 userid,UserStruct& ch);
	// followers or following
	void	launchGetFollow(u64 userid,const std::string& followtype, const std::string& nextToken = "-1");
	void	launchGetFavoritesRequest(u64 userid, const std::string& nextToken = "-1");
	void	launchGetTweetRequest(u64 userid, const std::string& username, const std::string& nextToken="-1");
	void	launchSearchTweetRequest(const std::string& hashtag, const std::string& nextToken = "-1");
	void	launchGetLikers(u64 tweetid, const std::string& nextToken = "-1");
	void	launchGetReplyers(u64 conversationID, const std::string& nextToken = "-1");
	void	launchGetRetweeters(u64 tweetid, const std::string& nextToken = "-1");

	static bool	LoadTweetsFile(std::vector<Twts>& tweetlist, const std::string& username, const std::string& fname="");
	static void	SaveTweetsFile(const std::vector<Twts>& tweetlist, const std::string& username, const std::string& fname = "");
	

	u32		getRequestCount()
	{
		return mRequestCount;
	}

	// utility

	template<typename T>
	static void	randomizeVector(std::vector<T>& v)
	{
		unsigned int s = v.size();
		if (s > 1)
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

	static void	FilterTweets(u64 authorid, std::vector<TwitterConnect::Twts>& twts,bool excludeRT,bool excludeReplies);

	static CoreItemSP	LoadLikersFile(u64 tweetid, const std::string& username);
	static void			SaveLikersFile(const std::vector<std::string>& tweetLikers, u64 tweetid, const std::string& username);

	static bool			LoadLikersFile(u64 tweetid, std::vector<u64>& likers);
	static void			SaveLikersFile(const std::vector<u64>& tweetLikers, u64 tweetid);

	static bool			LoadReplyersFile(u64 tweetid, std::vector<u64>& replyers);
	static void			SaveReplyersFile(const std::vector<u64>& tweetReplyers, u64 tweetid);

	static bool			LoadRetweetersFile(u64 tweetid, std::vector<u64>& rtweeters);
	static void			SaveRetweetersFile(const std::vector<u64>& RTers, u64 tweetid);


	static bool			LoadFavoritesFile(const std::string& username, std::vector<TwitterConnect::Twts>& fav);
	static bool			LoadFavoritesFile(u64 userid, std::vector<TwitterConnect::Twts>& fav);

	static void			SaveFavoritesFile(const std::string& username, const std::vector<TwitterConnect::Twts>& favs);
	static void			SaveFavoritesFile(u64 userid, const std::vector<TwitterConnect::Twts>& favs);

	// export tweet text
	static void			SaveTweet(const usString& text, u64 authorID, u64 tweetID);

	static std::string getHashtagFilename(const std::string& HashTag)
	{
		std::string result = "HashTag" + HashTag;
		if (HashTag[0] == '#')
		{
			result = "HashTag" + HashTag.substr(1);
		}
		return result;
	}

	static std::string getHashtagURL(const std::string& HashTag)
	{
		std::string result = HashTag;
		if (HashTag[0] == '#')
		{
			result = "%23" + HashTag.substr(1);
		}
		return result;
	}

	static void	initDates(const std::string& fromdate, const std::string& todate );

	static bool useDates()
	{
		return mUseDates;
	}

	static std::string getDate(u32 index)
	{
		return mDates[index].dateAsString;
	}

	static int	getDateDiffInMonth(const std::string& date1, const std::string& date2);

	static std::string getTodayDate();

protected:

	void	launchGenericRequest(float waitDelay);

	void	sendRequest(); // send waiting request
	void	resendRequest(); // send again same request

	WRAP_METHODS(resendRequest, sendRequest, thumbnailReceived);

	DECLARE_METHOD(getUserDetails);
	DECLARE_METHOD(getTweets);
	DECLARE_METHOD(getLikers);
	DECLARE_METHOD(getFollow);
	DECLARE_METHOD(getFavorites);
	DECLARE_METHOD(getReplyers);

	
	COREMODIFIABLE_METHODS(getUserDetails, getFollow, getTweets, getLikers, getFavorites, getReplyers);
	CoreItemSP	RetrieveJSON(CoreModifiable* sender);

	std::vector<std::pair<CMSP, std::pair<u64, UserStruct*>> >		mDownloaderList;

	static TwitterConnect* mInstance;

	bool			mWaitQuota = false;
	u32				mWaitQuotaCount = 0;
	unsigned int	mApiErrorCode = 0;

	std::vector<u64>	mCurrentIDVector;

	// return date in a u32 YYYYMMDD in result.first and seconds until start of this day in result.second
	static std::pair<u32, u32>	GetU32YYYYMMDD(const std::string& utcdate);

	static std::string creationDateToUTC(const std::string& created_date);

	void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);

	struct datestruct
	{
		u32	dateAsInt;
		std::string dateAsString;
	};

	// from and to dates
	static  datestruct	mDates[2];
	static  bool		mUseDates;



};