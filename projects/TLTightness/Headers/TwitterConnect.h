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

	class UserStruct
	{
	public:
		usString					mName = std::string("");
		unsigned int				mFollowersCount = 0;
		unsigned int				mFollowingCount = 0;
		unsigned int				mStatuses_count = 0;
		std::string					UTCTime = "";
		ThumbnailStruct				mThumb;
		std::vector<u64>			mFollowing;
		float						mW;
		u64							mID;
		// detailed stats
		u32							mLikerCount;
		u32							mLikerFollowerCount;
		u32							mLikerMainUserFollowerCount;
		u32							mLikerBothFollowCount;
		u32							mLikesCount;
	};

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

	static CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false);
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


	void		LaunchDownloader(u64 id, UserStruct& ch);
	bool		LoadUserStruct(u64 id, UserStruct& ch, bool requestThumb);
	void		SaveUserStruct(u64 id, UserStruct& ch);
	static std::string	GetUserFolderFromID(u64 id);
	static std::string	GetIDString(u64 id);
	static std::string  CleanURL(const std::string& url);
	static std::vector<u64>		LoadIDVectorFile(const std::string& filename, bool& fileExist);
	static void		SaveFollowingFile(u64 id, const std::vector<u64>& v);
	static void		SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename);

	template<typename T>
	static bool	LoadDataFile(const std::string& filename, std::vector<T>& loaded)
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
	static void	SaveDataFile(const std::string& filename, const std::vector<T>& saved)
	{
		SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
		if (L_File->mFile)
		{
			Platform_fwrite(saved.data(), 1, saved.size() * sizeof(T), L_File.get());
			Platform_fclose(L_File.get());
		}
	}

	bool		LoadThumbnail(u64 id, UserStruct& ch);


	void	launchUserDetailRequest(const std::string& UserName, UserStruct& ch,const std::string& signal="done");
	void	launchUserDetailRequest(u64 userid, UserStruct& ch, const std::string& signal = "done");
	void	launchGetFollowing(UserStruct& ch, const std::string& signal = "done");

	u32		getRequestCount()
	{
		return mRequestCount;
	}

protected:

	void	sendRequest(); // send waiting request
	void	resendRequest(); // send again same request

	WRAP_METHODS(resendRequest, sendRequest, thumbnailReceived);

	UserStruct* mCurrentUserStruct=nullptr;
	std::string	mSignal;


	DECLARE_METHOD(getUserDetails);
	DECLARE_METHOD(getFollowing);
	
	COREMODIFIABLE_METHODS(getUserDetails, getFollowing);
	CoreItemSP	RetrieveJSON(CoreModifiable* sender);

	std::vector<std::pair<CMSP, std::pair<u64, UserStruct*>> >		mDownloaderList;

	bool			mWaitQuota = false;
	unsigned int	mApiErrorCode = 0;

	std::string		mNextCursor = "-1";

	std::vector<u64>	mCurrentIDVector;

	void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);

};