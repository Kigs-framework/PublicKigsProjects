#pragma once

#include "DataDrivenBaseApplication.h"
#include "HTTPConnect.h"
#include "Texture.h"
#include "YoutubeConnect.h"
#include "GraphDrawer.h"

namespace Kigs
{
	namespace Http
	{
		class ResourceDownloader;
	}
	using namespace Kigs::DDriven;
	using namespace Kigs::Http;

	class YoutubeAnalyser : public DataDrivenBaseApplication
	{
	public:
		DECLARE_CLASS_INFO(YoutubeAnalyser, DataDrivenBaseApplication, Core);
		DECLARE_CONSTRUCTOR(YoutubeAnalyser);

		// give direct access to members
		friend class GraphDrawer;
		enum class dataType
		{
			Followers = 0,			
			Following = 1,
		};

	protected:

		// current application state 
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
			SP<Draw::Texture>			mTexture = nullptr;
			std::string					mURL;
		};

		class ChannelStruct
		{
		public:
			// channel name
			usString					mName;
			unsigned int				mSubscribersCount = 0;
			unsigned int				mNotSubscribedSubscribersCount = 0;
			unsigned int				mTotalSubscribers = 0;
			ThumbnailStruct				mThumb;
		};

		class PerChannelUserMap
		{
		public:
			PerChannelUserMap() :m(nullptr), mSubscribedCount(0), mSize(0)
			{

			}

			PerChannelUserMap(int SSize)
			{
				mSize = SSize;
				m = new unsigned char[mSize];
				memset(m, 0, mSize * sizeof(unsigned char));
			}
			PerChannelUserMap(const PerChannelUserMap& other)
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

			PerChannelUserMap& operator=(const PerChannelUserMap& other)
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

			~PerChannelUserMap()
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

			float	GetNormalisedSimilitude(const PerChannelUserMap& other);
			float	GetNormalisedAttraction(const PerChannelUserMap& other);
		};

		class UserStruct
		{
		public:
			// list of Channel IDs
			std::vector<std::string>	mPublicChannels;
			bool						mHasSubscribed = false;
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

		void	DrawForceBased();
		bool	mDrawForceBased = false;
		v2f		mThumbcenter;
		float	mForcedBaseStartingTime = 0.0f;

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

		void	prepareForceGraphData();

		void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);
		void	switchDisplay();
		void	switchForce();
		WRAP_METHODS(thumbnailReceived, switchDisplay, switchForce);

		// utility

		CoreItemSP	RetrieveJSON(CoreModifiable* sender);
		CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false);

		void		LaunchDownloader(const std::string& id, ChannelStruct& ch);

		void		SaveChannelStruct(const std::string& id, const ChannelStruct& ch);
		bool		LoadChannelStruct(const std::string& id, ChannelStruct& ch, bool requestThumb = false);
		void		SaveVideoList(const std::string& nextPage);
		void		SaveAuthorList(const std::string& nextPage, const std::string& videoID, unsigned int localParsedComments);
		void		SaveAuthor(const std::string& id);

		void		SaveStatFile();

		void	ProtectedInitSequence(const std::string& sequence) override;
		void	ProtectedCloseSequence(const std::string& sequence) override;

		// global data
		SP<HTTPConnect>									mGoogleConnect = nullptr;
		SP<HTTPAsyncRequest>							mAnswer = nullptr;
		std::vector<std::pair<CMSP, std::pair<std::string, ChannelStruct*>> >		mDownloaderList;
		std::string										mChannelName;
		std::string										mChannelID;
		ChannelStruct									mChannelInfos;
		unsigned int									mErrorCode = 0;

		unsigned int									mySubscribedWriters = 0;
		unsigned int									myPublicWriters = 0;
		unsigned int									mParsedComments = 0;
		unsigned int									myRequestCount = 0;

		//all treated users
		std::unordered_map<std::string, UserStruct>		mFoundUsers;

		//list of subscribed users
		std::vector<std::string>						mSubscriberList;
		std::unordered_map<std::string, PerChannelUserMap>	mChannelSubscriberMap;

		std::unordered_map<std::string, ChannelStruct*>	mFoundChannels;

		// display data
		std::unordered_map<std::string, CMSP>			mShowedChannels;
		SP<YoutubeConnect>								mYoutubeConnect = nullptr;
		CMSP											mMainInterface;
		SP<GraphDrawer>									mGraphDrawer = nullptr;
		double											mLastUpdate;

		// current process data
		std::vector<std::pair<std::string, std::string>>	mVideoListToProcess;
		unsigned int									mCurrentProcessedVideo = 0;
		std::vector<std::string>						mAuthorListToProcess;
		std::set<std::string>							mCurrentAuthorList;
		std::string										mCurrentProcessedUser;
		unsigned int									mCurrentVideoUserFound = 0;
		UserStruct										mCurrentUser;
		std::vector<std::pair<std::string, std::pair<usString, std::string>> >	mTmpUserChannels;

		bool mBadVideo = false;
		int  mNotSubscribedUserForThisVideo = 0;

		// get current time once at launch to compare with modified file date
		time_t     mCurrentTime = 0;

		// if a file is older ( in seconds ) than this limit, then it's considered as not existing ( to recreate )
		double		mOldFileLimit = 0.0;

		std::string	mFromDate = "";
		std::string	mToDate = "";


		enum Measure
		{
			Percent = 0,
			Similarity = 1,
			Normalized = 2,
			MEASURE_COUNT = 3
		};

		const std::string	mUnity[MEASURE_COUNT] = { "\%","sc","n" };


		Measure		mCurrentMeasure = Percent;
	};
}
