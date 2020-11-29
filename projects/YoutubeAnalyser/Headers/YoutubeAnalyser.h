#pragma once

#include <DataDrivenBaseApplication.h>
#include "HTTPConnect.h"
#include "Texture.h"

class ResourceDownloader;

class YoutubeAnalyser : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(YoutubeAnalyser, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(YoutubeAnalyser);

protected:

	// Youtube Data API key 
	std::string						mKey = "";

	// current application state ( 
	unsigned int					mState = 0;
	// general application parameters
	// wanted sample size
	unsigned int					mSubscribedUserCount = 100;
	unsigned int					mMaxChannelCount = 16;
	int								mMaxUserPerVideo = 0;
	float							mValidChannelPercent = 0.02f;
	// if set, don't get videolist from channel but from keyword
	std::string						mUseKeyword = "";

	class ThumbnailStruct
	{
	public:
		SP<Texture>					mTexture = nullptr;
		std::string					mURL;
	};

	class ChannelStruct
	{
	public:
		// channel name
		usString					mName;
		unsigned int				mSubscribersCount=0;
		unsigned int				mNotSubscribedSubscribersCount=0;
		unsigned int				mTotalSubscribers=0;
		ThumbnailStruct				mThumb;
	};

	class PerChannelUserMap
	{
	public:
		PerChannelUserMap():m(nullptr),mSubscribedCount(0)
		{
			
		}

		PerChannelUserMap(int SSize)
		{
			m = new unsigned char[SSize];
			memset(m, 0, SSize * sizeof(unsigned char));
		}
		PerChannelUserMap(const PerChannelUserMap& other)
		{
			m = other.m;
			mSubscribedCount = other.mSubscribedCount;
		}

		PerChannelUserMap& operator=(const PerChannelUserMap& other)
		{
			m = other.m;
			mSubscribedCount = other.mSubscribedCount;
			return *this;
		}

		~PerChannelUserMap()
		{
			delete[] m;
		}

		void	SetSubscriber(int index)
		{
			m[index] = 1;
			mSubscribedCount++;
		}

		unsigned char*	m=nullptr;
		unsigned int	mSubscribedCount=0;
	private:

	};

	class UserStruct 
	{
	public:
		// list of Channel IDs
		std::vector<std::string>	mPublicChannels;
		bool						mHasSubscribed=false;
		bool	isSubscriberOf(const std::string& chan) const
		{
			for (auto& c : mPublicChannels)
			{
				if (c == chan)
				{
					return true;
				}
			}
			return false;
		}
	};


	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;

	DECLARE_METHOD(getChannelID);
	DECLARE_METHOD(getChannelStats);
	DECLARE_METHOD(getVideoList);
	DECLARE_METHOD(getCommentList);
	DECLARE_METHOD(getUserSubscribtion);

	COREMODIFIABLE_METHODS(getChannelID, getVideoList, getCommentList, getUserSubscribtion, getChannelStats);

	void	refreshAllThumbs();

	void	prepareCloudGraphData();

	void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);
	void	switchDisplay();
	WRAP_METHODS(thumbnailReceived, switchDisplay);

	// utility

	CoreItemSP	RetrieveJSON(CoreModifiable* sender);
	CoreItemSP	LoadJSon(const std::string& fname,bool utf16=false);

	void		LaunchDownloader(const std::string& id, ChannelStruct& ch);

	void		SaveChannelStruct(const std::string& id, const ChannelStruct& ch);
	bool		LoadChannelStruct(const std::string& id, ChannelStruct& ch,bool requestThumb=false);
	void		SaveVideoList(const std::string& nextPage);
	void		SaveAuthorList(const std::string& nextPage, const std::string& videoID);
	void		SaveAuthor(const std::string& id);

	void		SaveStatFile();

	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;

	// global data
	SP<HTTPConnect>									mGoogleConnect = nullptr;
	SP<HTTPAsyncRequest>							mAnswer = nullptr;
	std::vector<std::pair<CMSP, std::pair<std::string,ChannelStruct*>> >		mDownloaderList;
	std::string										mChannelName;
	std::string										mChannelID;
	ChannelStruct									mChannelInfos;
	unsigned int									mErrorCode = 0;

	unsigned int									mySubscribedWriters = 0;
	unsigned int									myPublicWriters = 0;
	unsigned int									myParsedComments = 0;
	unsigned int									myRequestCount = 0;

	//all treated users
	std::unordered_map<std::string, UserStruct>		mFoundUsers;

	//list of subscribed users
	std::vector<std::string>						mSubscriberList;
	std::unordered_map<std::string, PerChannelUserMap>	mChannelSubscriberMap;

	std::unordered_map<std::string, ChannelStruct*>	mFoundChannels;

	// display data
	std::unordered_map<std::string, CMSP>			mShowedChannels;
	CMSP											mMainInterface;
	double											mLastUpdate;

	// current process data
	std::vector<std::pair<std::string, int>>		mVideoListToProcess;
	unsigned int									mCurrentProcessedVideo = 0;
	std::vector<std::string>						mAuthorListToProcess;
	std::set<std::string>							mCurrentAuthorList;
	std::string										mCurrentProcessedUser;
	unsigned int									mCurrentVideoUserFound;
	UserStruct										mCurrentUser;
	std::vector<std::pair<std::string, std::pair<usString, std::string>> >	mTmpUserChannels;

	bool mBadVideo = false;
	int  mNotSubscribedUserForThisVideo = 0;

	// get current time once at launch to compare with modified file date
	time_t     mCurrentTime = 0;

	// if a file is older ( in seconds ) than this limit, then it's considered as not existing ( to recreate )
	double		mOldFileLimit=0.0;

	
	// test
	bool		mShowInfluence=false;
};
