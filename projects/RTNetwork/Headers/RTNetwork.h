#pragma once

#include <DataDrivenBaseApplication.h>
#include "Texture.h"
#include "HTTPConnect.h"
#include "TwitterAccount.h"

class AnonymousModule;

class RTNetwork : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(RTNetwork, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(RTNetwork);

	const std::string& getFromDate() const 
	{
		return mFromDate;
	}
	const std::string& getToDate() const
	{
		return mToDate;
	}

	std::map<u64, std::string>& getUserIDToNameMap()
	{
		return mUserIDToNameMap;
	}
	std::map<std::string, u64>& getNameToUserIDMap()
	{
		return mNameToUserIDMap;
	}

	void	updateUserNameAndIDMaps(const std::string& uname, u64 uid);

	std::string	getTwitterAccountForURL(const std::string& url);

protected:

	SP<HTTPConnect>									mTwitterConnect = nullptr;
	SP<HTTPAsyncRequest>							mAnswer = nullptr;

	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;

	DECLARE_METHOD(getTweets);
	DECLARE_METHOD(getTweetAuthor);
	DECLARE_METHOD(getUserDetails);
	DECLARE_METHOD(getRetweetAuthors);
	COREMODIFIABLE_METHODS(getTweets,  getUserDetails, getTweetAuthor, getRetweetAuthors);

	void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);
	void	webscrapperNavigationComplete(CoreModifiable* sender);

	WRAP_METHODS(thumbnailReceived,webscrapperNavigationComplete);

	CoreItemSP	RetrieveJSON(CoreModifiable* sender);

	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;
	static CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false,bool checkfileDate=true);
	static void			SaveJSon(const std::string& fname,const CoreItemSP& json, bool utf16 = false);

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

	std::string			mFromDate;
	std::string			mToDate;

	std::string			mStartUserName;
	TwitterAccount*		mCurrentTreatedAccount=nullptr;

	enum AppStates
	{
		WAIT_STATE =					0,
		GET_USER_DETAILS =				1,
		GET_TWEETS =					2,
		WAIT_STARTUSERID =				3,
		TREAT_QUOTED =					4,
		TREAT_RETWEETERS =				5,
		TREAT_USERNAME_REQUEST =		6,
		RESOLVE_URL =					7,
		EVERYTHING_DONE =				17
	};
	// current application state 
	std::vector< AppStates>					mState;

	CMSP			mMainInterface;
	u64				mStartUserID;


	TwitterAccount* getUser(u64 uID)
	{
		auto f = mUserAccountList.find(uID);
		if (f != mUserAccountList.end())
			return ((*f).second);


		TwitterAccount* toInsert = new TwitterAccount(this);
		toInsert->mID = uID;
		mUserAccountList.insert({ uID,toInsert });
		
		return toInsert;
	}

	void clearUsers()
	{
		for (auto i : mUserAccountList)
		{
			delete i.second;
		}
		for (auto dt : mThumbs)
		{
			delete dt;
		}

		mThumbs.clear();

		mUserAccountList.clear();
		mUserIDToNameMap.clear();
	}


	std::map<u64,TwitterAccount*>				mUserAccountList;
	std::map<u64, std::string>					mUserIDToNameMap;
	std::map<std::string, u64>					mNameToUserIDMap;
	std::vector<TwitterAccount*>				mAnalysedAccountList;

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

	void		LaunchDownloader(u64 id, TwitterAccount::UserStruct& ch);
	bool		LoadUserStruct(u64 id, TwitterAccount::UserStruct& ch, bool requestThumb);
	void		SaveUserStruct(u64 id, TwitterAccount::UserStruct& ch);

	bool		LoadTweetsFile(const std::string& fname = "");
	void		SaveTweetsFile(const std::string& fname = "");


	std::vector<u64>		LoadIDVectorFile(const std::string& filename);
	void					SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename);

	bool		LoadThumbnail(u64 id, TwitterAccount::UserStruct& ch);

	static std::string	GetUserFolderFromID(u64 id);
	static std::string	GetIDString(u64 id);
	static std::string  CleanURL(const std::string& url);

	// get current time once at launch to compare with modified file date
	static time_t		mCurrentTime;
	// if a file is older ( in seconds ) than this limit, then it's considered as not existing ( to recreate )
	static double		mOldFileLimit;
	double				mLastUpdate;

	bool				mWaitQuota = false;
	unsigned int		mWaitQuotaCount = 0;


	unsigned int		myRequestCount = 0;
	double				mStartWaitQuota=0.0;

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

	TwitterAccount* chooseNextAccountToTreat();

	static bool checkValidFile(const std::string& fname, SmartPointer<::FileHandle>& filenamehandle, double OldFileLimit);

	double		mNextRequestDelay = 0.0;
	double		mLastRequestTime = 0.0;

	std::vector<u64>			mUserDetailsAsked;

	int							mCurrentYear=0;

	void	refreshShowed();

	unsigned int				mApiErrorCode=0;
	std::vector<std::pair<CMSP, std::pair<u64, TwitterAccount::UserStruct*>> >		mDownloaderList;

	class displayThumb;

	class thumbLink
	{
	public:
		displayThumb*		mThumbs[2];
		float				mLength=50.0f;
		CMSP				mDisplayedLink;
	};

	class displayThumb
	{
	public:
		TwitterAccount*				mAccount;
		std::string					mName;
		CMSP						mThumb;
		std::vector<thumbLink*>		mLink;
		v2f							mAcceleration = {0.0f,0.0f};
		v2f							mSpeed = { 0.0f,0.0f };
		v2f							mPos = { 0.0f,0.0f };
		float						mRadius = 50.0f;
		v3f							mBackColor;
	};

	std::vector<v3f>	mAlreadyFoundColors;

	v3f getRandomColor();

	displayThumb* setupThumb(TwitterAccount* account);
	void		setupLinks(displayThumb* account);
	
	void	moveThumbs();

	std::vector<std::pair<displayThumb*, u32>> getValidLinks(u64 uID,bool includeEmpty=true);

	void	updateLinks();

	std::map<std::pair<u64, u64>, thumbLink>		mLinks;
	std::vector< displayThumb*>						mThumbs;


	// resolve urls

	AnonymousModule*	 mWebScraperModule = nullptr;
	SP<CoreModifiable>	 mWebScraper = nullptr;

	std::map<std::string, std::string>	mWebToTwitterMap;

	void		saveWebToAccountFile();
	void		loadWebToAccountFile();
	
};
