#pragma once
#include "Texture.h"
#include "Timer.h"
class RTNetwork;

class TwitterAccount
{
protected:

	friend class RTNetwork;

	class ThumbnailStruct
	{
	public:
		SP<Texture>					mTexture = nullptr;
		std::string					mURL = "";
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
		
	};

	struct tweet
	{
		u64									mID;
		u32									mRTCount;
		u32									mQuoteCount;
		std::vector<std::pair<u64,u32>>		mReferences;
		std::string							mRTedUser;
		std::vector<std::string>			mURLs;
	};

	UserStruct			mUserStruct;
	std::vector<tweet>	mTweetList;
	u64					mID;

	std::string			mNextTweetsToken="-1";

	std::vector<u64>			mTweetToRequestList;
	std::set<std::string>		mUserNameRequestList;
	std::set<std::string>		mYoutubeVideoList;

	
	std::map<u64, u32>	mCountHasRT; // users retweeted by this account
	std::map<u64, u32>	mCountWasRT; // users who retweeted a tweet from this account
	u32					mHasRetweetCount=0;
	u32					mWasRetweetCount=0;
	u32					mNotRetweetCount=0;
	u32					mOwnRetweetCount = 0;
	u32					mAllTweetCount = 0;
	u32					mConnectionCount =0; // interaction count
	float				mSourceCoef=0.0f;

	// build network and sort it by more interaction to less interaction
	// u64 = ID
	// u32 = hasRT
	// u32 = wasRT
	std::vector < std::pair<u64, std::pair<u32,u32>> >	mSortedNetwork;

	CoreItemSP	loadTweetsFile();
	void		saveTweetsFile(CoreItemSP toSave);

	CoreItemSP	loadTweetUserFile(u64 twtid);
	void		saveTweetUserFile(u64 twtid, const std::string& username);

	CoreItemSP	loadRetweeters(u64 twtid);
	void		saveRetweeters(u64 twtid, CoreItemSP tosave);

	static CoreItemSP	loadUserID(const std::string& uname);
	static void		saveUserID(const std::string& uname, u64 id);

	CoreItemSP	loadURL(const std::string& shortURL);
	void		saveURL(const std::string& shortURL, const std::string& fullURL);

	CoreItemSP		loadYoutubeFile(const std::string& videoID);
	void			saveYoutubeFile(const std::string& videoID, const std::string& channelID);

	void		updateTweetList(CoreItemSP currentTwt);

	RTNetwork* mSettings = nullptr;

	void		buildNetworkList();

	int			mDepth = -1;

	bool		mIsMandatory = false;

	std::vector<std::string>	mNeedURLs;

	bool		mAddToNetwork = true;

public:

	void	setMandatory(bool m)
	{
		mIsMandatory = m;
	}

	bool isMandatory()
	{
		return mIsMandatory;
	}
	void	setDepth(int d)
	{
		mDepth = d;
	}

	int		getDepth() const
	{
		return mDepth;
	}

	u32		getConnectionCount()
	{
		return mConnectionCount;
	}

	u32		getHasRTCount()
	{
		return mHasRetweetCount;
	}

	u32		getTweetCount()
	{
		return mAllTweetCount;
	}

	u32		getWasRTCount()
	{
		return mWasRetweetCount;
	}

	TwitterAccount(RTNetwork* settings) : mSettings(settings)
	{

	}

	bool		needMoreTweets();
	void		addTweets(CoreItemSP json, bool addtofile);
	void		updateEmbeddedURLList(); // decode urls 
	void		updateTweetRequestList(); // search quoted tweet author
	void		updateRetweeterList(); // search retweeters authors
	void		updateUserNameRequest();
	void		updateStats();

	const std::vector < std::pair<u64, std::pair<u32, u32>> >& getSortedNetwork() const
	{
		return mSortedNetwork;
	}

	u32 getLinkCoef(TwitterAccount* other);
	u32 getLinkCoef(u64 otherID);

	u32 getHasRTCoef(u64 otherID);
	u32 getWasRTCoef(u64 otherID);

	bool	needAddToNetwork()
	{
		return mAddToNetwork;
	}

	void	setNeedAddToNetwork(bool toset)
	{
		mAddToNetwork = toset;
	}
};
