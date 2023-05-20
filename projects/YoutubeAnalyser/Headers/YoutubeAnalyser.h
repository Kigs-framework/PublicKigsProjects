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


	protected:

		void	createFSMStates();


		

		void	DrawForceBased();
		bool	mDrawForceBased = false;
		v2f		mThumbcenter;
		float	mForcedBaseStartingTime = 0.0f;

		void	ProtectedInit() override;
		void	ProtectedUpdate() override;
		void	ProtectedClose() override;

		void	refreshAllThumbs();

		void	prepareForceGraphData();

		void	switchDisplay();
		void	switchForce();
		void	mainChannelID(const std::string& id);
		void	getChannelStats(const YoutubeConnect::ChannelStruct& chan);
		void	requestDone();

		WRAP_METHODS( switchDisplay, switchForce, mainChannelID, getChannelStats);

		// utility
		CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false);

		void		SaveStatFile();

		void	ProtectedInitSequence(const std::string& sequence) override;
		void	ProtectedCloseSequence(const std::string& sequence) override;

		// global data
		SP<HTTPConnect>									mGoogleConnect = nullptr;
		std::string										mChannelName;
		std::string										mChannelID;

		YoutubeConnect::ChannelStruct					mChannelInfos;

		unsigned int									mErrorCode = 0;

		unsigned int									mySubscribedWriters = 0;
		unsigned int									myPublicWriters = 0;
		unsigned int									mParsedComments = 0;
		unsigned int									myRequestCount = 0;

		//all treated users
		std::unordered_map<std::string, YoutubeConnect::UserStruct>		mFoundUsers;

		// all subscribed authors infos
		std::unordered_map<std::string, YoutubeConnect::ChannelStruct>	mSubscribedAuthorInfos;

		// all not subscribed authors infos
		std::unordered_map<std::string, YoutubeConnect::ChannelStruct>	mNotSubscribedAuthorInfos;


		//list of subscribed users
		std::vector<std::string>							mSubscriberList;
		std::unordered_map<std::string, PerChannelUserMap>	mChannelSubscriberMap;

		std::unordered_map<std::string, YoutubeConnect::ChannelStruct*>	mFoundChannels;

		// display data
		std::unordered_map<std::string, CMSP>			mShowedChannels;
		SP<YoutubeConnect>								mYoutubeConnect = nullptr;
		CMSP											mMainInterface;
		SP<GraphDrawer>									mGraphDrawer = nullptr;
		double											mLastUpdate;

		// current process data
		std::vector<std::pair<std::string, std::string>>	mVideoListToProcess;
		std::set<std::string>								mAlreadyTreatedAuthors;
		unsigned int										mCurrentProcessedVideo = 0;
		std::string											mCurrentProcessedUser;
		unsigned int										mCurrentVideoUserFound = 0;
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

		CMSP												mFsm;
		std::unordered_map<KigsID, SP<CoreFSMTransition>>	mTransitionList;

		// wait request was treated
		maBool	mNeedWait = BASE_ATTRIBUTE(NeedWait, false);
	};
}
