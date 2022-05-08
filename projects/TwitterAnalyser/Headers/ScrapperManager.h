#pragma once
#include "CoreModifiable.h"
#include "AnonymousModule.h"
#include "CoreBaseApplication.h"

class ScrapperManager : public CoreModifiable
{

public:

	enum ScraperStates
	{
		GET_LIKES = 0,
		SCROLL_LIKES = 1,
	};

protected:

	// scrapper module
	SP<AnonymousModule> mWebScraperModule = nullptr;
	SP<CoreModifiable> mWebScraper = nullptr;
	void	ReadScripts();
	std::string	mWalkInitScript = "";
	std::string mCallWalkScript = "";

	void InitModifiable() override;

	void	treatWebScraperMessage(CoreModifiable* sender, std::string msg);
	void	LaunchScript(CoreModifiable* sender);

	WRAP_METHODS(treatWebScraperMessage, LaunchScript);

	ScraperStates					mScraperState;
	std::string						mNextScript;

	struct tmpScrappedUserName
	{
		std::string		userName;
		u32				foundCount;
	};

	std::vector<tmpScrappedUserName>								mCurrentScrappedUserNameList;
	std::vector<std::string>										mTweetLikers;
	std::string														mLastRetrievedLiker;
	std::map<std::string, unsigned int>								mTweetLikersMap;
	u32																mListEndReachCount=0;

	std::string														mCurrentURL;

	void	protectedLaunchScrap();

	double	mLastActiveTime = -1.0f;

public:

	DECLARE_CLASS_INFO(ScrapperManager, CoreModifiable, ScrapperManager);
	DECLARE_CONSTRUCTOR(ScrapperManager);

	void	launchScrap(const std::string& username, const std::string& tweetID);

	std::vector<std::string>& getTweetLikers()
	{
		return mTweetLikers;
	}

	double getInactiveTime();
};