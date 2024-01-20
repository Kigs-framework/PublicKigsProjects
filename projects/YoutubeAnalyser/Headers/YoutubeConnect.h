#pragma once
#include "Texture.h"
#include "HTTPConnect.h"
#include "CoreBaseApplication.h"

namespace Kigs
{
	using namespace Kigs::Core;
	using namespace Kigs::Draw;
	using namespace Kigs::Http;
	using namespace Kigs::File;

	class YoutubeConnect : public CoreModifiable
	{
	protected:
		// bearer management
		std::vector<std::string>	mYoutubeBear;
		unsigned int				mCurrentBearer = 0;

		double						mNextRequestDelay = 0.0;
		double						mLastRequestTime = 0.0;

		static double				mOldFileLimit;

		// get current time once at launch to compare with modified file date
		static time_t				mCurrentTime;
		unsigned int				mRequestCount = 0;

	public:

		DECLARE_CLASS_INFO(YoutubeConnect, CoreModifiable, YoutubeConnect);
		DECLARE_CONSTRUCTOR(YoutubeConnect);

		class ThumbnailStruct
		{
		public:
			SP<Texture>					mTexture = nullptr;
			std::string					mURL = "";
		};

		struct ChannelStruct
		{
			// channel name
			usString					mName;
			std::string					mID;
			std::string					mCreationDate;
			unsigned int				mTotalSubscribers = 0;
			unsigned int				mViewCount = 0;
			unsigned int				mVideoCount = 0;
			ThumbnailStruct				mThumb;

			// dynamic values (not loaded / saved)
			unsigned int				mSubscribersCount = 0;
			unsigned int				mNotSubscribedSubscribersCount = 0;
		};

		struct videoStruct
		{
			std::string					mID;
			std::string					mChannelID;
			usString					mName;
		};

		struct YTComment
		{
			u64		mAuthorID;		
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

		std::string	getCurrentBearer()
		{
			return mYoutubeBear[mCurrentBearer];
			mCurrentBearer = (mCurrentBearer + (unsigned int)1) % mYoutubeBear.size();
		}

		// HTTP Request management
		SP<HTTPConnect>									mYoutubeConnect = nullptr;
		SP<HTTPAsyncRequest>							mAnswer = nullptr;

		static CoreItemSP	LoadJSon(const std::string& fname, bool useOldFileLimit = true, bool utf16 = false);
		static void			SaveJSon(const std::string& fname, const CoreItemSP& json, bool utf16 = false);

		static bool			checkValidFile(const std::string& fname, SmartPointer<Kigs::File::FileHandle>& filenamehandle, double OldFileLimit);

		// manage wait time between requests
		void				RequestLaunched(double toWait)
		{
			mNextRequestDelay = toWait / mYoutubeBear.size();
			mLastRequestTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
		}

		bool				CanLaunchRequest()
		{
			double dt = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime;
			if (dt > mNextRequestDelay)
			{
				return true;
			}
			return false;
		}

		float				getDelay()
		{
			double dt = mNextRequestDelay - (KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mLastRequestTime);
			return dt;
		}

		void					initBearer(CoreItemSP f);
		void					initConnection(double oldfiletime);

		static bool				LoadChannelID(const std::string& channelName, std::string& channelID);
		static void				SaveChannelID(const std::string& channelName, const std::string& channelID);

		static bool				LoadChannelStruct(const std::string& channelID, ChannelStruct& ch, bool requestThumb);
		static void				SaveChannelStruct(const ChannelStruct& ch);

		static CoreItemSP		LoadVideoList(const std::string& channelID);
		static void				SaveVideoList(const std::string& channelID, const std::vector<videoStruct>& videoList,const std::string& nextPage);

		static CoreItemSP		LoadCommentList(const std::string& channelID, const std::string& videoID);
		static void				SaveCommentList(const std::string& channelID, const std::string& videoID, const std::vector<std::string>& authorList, const std::string& nextPage);

		static CoreItemSP		LoadAuthorFile(const std::string& authorID);
		static void				SaveAuthorFile(const std::string& authorID, const std::vector<std::string>& subscrList,const std::string& nextPage);


		static void				LaunchDownloader(ChannelStruct& ch);
		
		static std::string		CleanURL(const std::string& url);

		template<typename T>
		static bool				LoadDataFile(const std::string& filename, std::vector<T>& loaded, bool useOldFileLimit = true)
		{
			SmartPointer<Kigs::File::FileHandle> L_File;

			if (checkValidFile(filename, L_File, useOldFileLimit ? mOldFileLimit : 0.0))
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
		static void				SaveDataFile(const std::string& filename, const std::vector<T>& saved)
		{
			SmartPointer<Kigs::File::FileHandle> L_File = Kigs::File::Platform_fopen(filename.c_str(), "wb");
			if (L_File->mFile)
			{
				Platform_fwrite(saved.data(), 1, saved.size() * sizeof(T), L_File.get());
				Platform_fclose(L_File.get());
			}
		}

		void					launchGetChannelID(const std::string& channelName);
		void					launchGetChannelStats(const std::string& channelID);
		void					launchGetVideo(const std::string& channelID, const std::string& nextPage);
		void					launchGetComments(const std::string& channelID, const std::string& videoID, const std::string& nextPage);
		void					launchGetSubscriptions(const std::string& authorID, const std::string& nextPage);

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

		static void	initDates(const std::string& fromdate, const std::string& todate);

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

		WRAP_METHODS( thumbnailReceived);


		DECLARE_METHOD(getChannelID);
		DECLARE_METHOD(getChannelStats);
		DECLARE_METHOD(getVideoList);
		DECLARE_METHOD(getCommentList);
		DECLARE_METHOD(getUserSubscription);

		COREMODIFIABLE_METHODS(getChannelID, getChannelStats, getVideoList, getCommentList, getUserSubscription);

		CoreItemSP	RetrieveJSON(CoreModifiable* sender);

		std::vector<std::pair<CMSP, std::pair<std::string, ChannelStruct*>> >		mDownloaderList;

		static YoutubeConnect* mInstance;

		unsigned int		mErrorCode = 0;

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
		static  datestruct		mDates[2];

		static  bool		mUseDates;

	};
}