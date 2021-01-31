#pragma once

#include <DataDrivenBaseApplication.h>
#include "Texture.h"
#include "HTTPConnect.h"

class AnonymousModule;

class TwitterAnalyser : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(TwitterAnalyser, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(TwitterAnalyser);

protected:

	SP<HTTPConnect>									mTwitterConnect = nullptr;
	SP<HTTPAsyncRequest>							mAnswer = nullptr;

	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;


	DECLARE_METHOD(getFollowers);
	DECLARE_METHOD(getFavorites);
	DECLARE_METHOD(getTweets);
	DECLARE_METHOD(getFollowing);
	DECLARE_METHOD(getUserDetails);
	COREMODIFIABLE_METHODS(getFavorites,getFollowers,getTweets,  getFollowing, getUserDetails);

	void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);
	void	switchDisplay();
	void	switchForce();
	void	treatWebScraperMessage(CoreModifiable* sender, std::string msg);
	void	LaunchScript(CoreModifiable* sender);

	WRAP_METHODS(thumbnailReceived, switchDisplay, switchForce, treatWebScraperMessage, LaunchScript);

	CoreItemSP	RetrieveJSON(CoreModifiable* sender);

	
	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;
	CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false);
	void		SaveJSon(const std::string& fname,const CoreItemSP& json, bool utf16 = false);

	std::vector<std::string>	mTwitterBear;
	unsigned int				mCurrentBearer = 0;
	unsigned int				NextBearer()
	{
		mCurrentBearer = (mCurrentBearer + (unsigned int)1) % mTwitterBear.size();
		return mCurrentBearer;
	}
	unsigned int				CurrentBearer()
	{
		return mCurrentBearer;
	}
	std::string mUserName;

	unsigned int mUserPanelSize;
	unsigned int mWantedTotalPanelSize=100000;
	unsigned int mMaxUserCount;
	float		 mValidUserPercent;

	// current application state 
	unsigned int					mState = WAIT_STATE;


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

	std::unordered_map<u64, PerAccountUserMap>	mAccountSubscriberMap;

	void	prepareForceGraphData();
	class ThumbnailStruct
	{
	public:
		SP<Texture>					mTexture = nullptr;
		std::string					mURL="";
	};

	class UserStruct
	{
	public:
		usString					mName="";
		unsigned int				mFollowersCount = 0;
		unsigned int				mFollowingCount = 0;
		unsigned int				mStatuses_count = 0;
		std::string					UTCTime = "";
		ThumbnailStruct				mThumb;
		std::vector<u64>			mFollowing;
		float						mW;
	};

	void	DrawForceBased();
	bool	mDrawForceBased = false;
	v2f		mThumbcenter;
	float	mForcedBaseStartingTime = 0.0f;

	CMSP			mMainInterface;
	u64				mUserID;
	UserStruct		mCurrentUser;

	std::vector<std::pair<CMSP, std::pair<u64, UserStruct*>> >						mDownloaderList;
	std::map<u64, std::pair<unsigned int, UserStruct>>				mFollowersFollowingCount;
	unsigned int													mCurrentLikerFollowerCount;

	std::unordered_map<u64, CMSP>			mShowedUser;

	// list of followers
	std::vector<u64>												mFollowers;
	// list of liked tweets
	struct Twts
	{
		u64		mTweetID;
		u32		mLikeCount;
		u32		mRetweetCount;
	};
	std::vector<Twts>												mTweets;
	u32																mMaxLikersPerTweet=0;
	u32																mCurrentTreatedTweetIndex=0;
	std::string														mNextScript;
	struct tmpScrappedUserName
	{
		std::string		userName;
		u32				foundCount;
	};
	std::vector<tmpScrappedUserName>								mCurrentScrappedUserNameList;
	std::vector<std::string>										mTweetLikers;
	u32																mCurrentTreatedLikerIndex=0;
	u32																mValidTreatedLikersForThisTweet = 0;
	std::map<std::string, u32>										mFoundLiker;

	struct favoriteStruct
	{
		u64		tweetID;
		u64		userID;
		u32		likes_count;
		u32		retweet_count;
	};

	// list of favorites per liker
	std::map<std::string,std::vector<favoriteStruct>>					mFavorites;
	std::map <std::string, std::map<u64, float> >						mWeightedFavorites;

	float	getTotalWeightForAccount(u64 toget)
	{
		float w = 0.0f;

		for (const auto& wf : mWeightedFavorites)
		{
			auto found = wf.second.find(toget);
			if(found != wf.second.end())
			{
				w += (*found).second;
			}
		}

		return w;
	}

	bool	LoadFavoritesFile(const std::string& username);
	void	SaveFavoritesFile(const std::string& username);

	template<typename T>
	bool	LoadDataFile(const std::string& filename, std::vector<T>& loaded)
	{
		SmartPointer<::FileHandle> L_File;
		
		if (checkValidFile(filename, L_File, mOldFileLimit))
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
	void	SaveDataFile(const std::string& filename, const std::vector<T>& saved)
	{
		SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
		if (L_File->mFile)
		{
			Platform_fwrite(saved.data(), 1, saved.size() * sizeof(T), L_File.get());
			Platform_fclose(L_File.get());
		}
	}

	std::map<u64,std::vector<u64>>									mCheckedFollowerList;
	std::map<std::string, std::vector<u64>>							mCheckedLikersList;
	unsigned int													mTreatedFollowerIndex = 0;
	unsigned int													mTreatedFollowerCount = 0;
	unsigned int													mValidFollowerCount = 0;
	unsigned int													mFakeFollowerCount = 0;
	unsigned int													mCurrentStartingFollowerList = 0;
	void	NextTreatedFollower()
	{
		mTreatedFollowerIndex += mFollowers.size() / 7;
		if (mTreatedFollowerIndex >= mFollowers.size())
		{
			mCurrentStartingFollowerList++;
			mTreatedFollowerIndex = mCurrentStartingFollowerList;
		}
		mTreatedFollowerCount++;
	}

	bool	isSubscriberOf(u64 follower, u64 account) const
	{
		const auto& found = mCheckedFollowerList.find(follower);
		if (found != mCheckedFollowerList.end())
		{
			for (auto& c : (*found).second)
			{
				if (c == account)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool	isLikerOf(const std::string& liker, u64 account) const
	{
		const auto& found = mCheckedLikersList.find(liker);
		if (found != mCheckedLikersList.end())
		{
			for (auto& c : (*found).second)
			{
				if (c == account)
				{
					return true;
				}
			}
		}
		return false;
	}


	// manage following for one user
	std::vector<u64>												mCurrentFollowing;

	void		LaunchDownloader(u64 id, UserStruct& ch);
	bool		LoadUserStruct(u64 id, UserStruct& ch, bool requestThumb);
	void		SaveUserStruct(u64 id, UserStruct& ch);

	bool		LoadFollowersFile();
	void		SaveFollowersFile();

	bool		LoadTweetsFile();
	void		SaveTweetsFile();

	bool		LoadFollowingFile(u64 id);
	void		SaveFollowingFile(u64 id);

	CoreItemSP		LoadLikersFile(u64 tweetid);
	void		SaveLikersFile(u64 tweetid);

	void		UpdateStatistics();
	void		UpdateLikesStatistics();

	std::vector<u64>		LoadIDVectorFile(const std::string& filename);
	void					SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename);

	bool		LoadThumbnail(u64 id, UserStruct& ch);
	void		refreshAllThumbs();

	static std::string	GetUserFolderFromID(u64 id);
	static std::string	GetIDString(u64 id);
	static std::string  CleanURL(const std::string& url);

	// get current time once at launch to compare with modified file date
	time_t     mCurrentTime = 0;
	// if a file is older ( in seconds ) than this limit, then it's considered as not existing ( to recreate )
	double		mOldFileLimit = 0.0;
	double		mLastUpdate;
	bool		mShowInfluence = false;
	bool		mWaitQuota = false;
	unsigned int	mWaitQuotaCount = 0;

	std::string	mFollowersNextCursor = "-1";

	unsigned int									myRequestCount = 0;
	double		mStartWaitQuota=0.0;

	// manage wait time between requests

	void	RequestLaunched(double toWait)
	{
		mNextRequestDelay = toWait/ mTwitterBear.size();
		mLastRequestTime = GetApplicationTimer()->GetTime();
	}

	bool	CanLaunchRequest()
	{
		double dt = GetApplicationTimer()->GetTime() - mLastRequestTime;
		if (dt > mNextRequestDelay)
		{
			return true;
		}
		return false;
	}

	bool checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle, double OldFileLimit);

	double		mNextRequestDelay = 0.0;
	double		mLastRequestTime = 0.0;

	std::vector<u64>			mUserDetailsAsked;

	int							mCurrentYear=0;

	enum AppStates
	{
		WAIT_STATE						= 0,
		GET_FOLLOWERS_INIT				= 1,
		GET_FOLLOWERS_CONTINUE			= 2,
		CHECK_INACTIVES					= 3,
		TREAT_FOLLOWER					= 4,
		UPDATE_STATISTICS				= 5,
		GET_USER_DETAILS_FOR_CHECK		= 6,
		WAIT_USER_DETAILS_FOR_CHECK		= 7,
		GET_USER_DETAILS				= 8,
		GET_TWEET_LIKES					= 9,
		GET_USER_FAVORITES				= 10,
		UPDATE_LIKES_STATISTICS			= 11,
		GET_USER_ID						= 12,
		WAIT_USER_ID					= 13,
		EVERYTHING_DONE					= 14
	};

	enum ScraperStates
	{
		GET_LIKES						=0,
		SCROLL_LIKES					=1,
	};
	ScraperStates													mScraperState;

	unsigned int				mApiErrorCode=0;

	bool						mUseLikes = false;

	AnonymousModule* mWebScraperModule=nullptr;
	SP<CoreModifiable> mWebScraper=nullptr;
};
