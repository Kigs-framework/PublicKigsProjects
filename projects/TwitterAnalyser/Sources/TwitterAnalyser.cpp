#include <TwitterAnalyser.h>
#include <FilePathManager.h>
#include <NotificationCenter.h>
#include "HTTPRequestModule.h"
#include "JSonFileParser.h"
#include "CoreFSM.h"
#include "TextureFileManager.h"
#include "CommonTwitterFSMStates.h"

IMPLEMENT_CLASS_INFO(TwitterAnalyser);

IMPLEMENT_CONSTRUCTOR(TwitterAnalyser)
{

}

void		TwitterAnalyser::commonStatesFSM()
{
	SP<CoreFSM> fsm = mFsm;

	// go to wait state (push)
	SP<CoreFSMTransition> waittransition = KigsCore::GetInstanceOf("waittransition", "CoreFSMOnValueTransition");
	waittransition->setValue("TransitionBehavior", "Push");
	waittransition->setValue("ValueName", "NeedWait");
	waittransition->setState("Wait");
	waittransition->Init();

	// pop when objective is reached (pop)
	SP<CoreFSMTransition> popwhendone = KigsCore::GetInstanceOf("popwhendone", "CoreFSMOnMethodTransition");
	popwhendone->setValue("TransitionBehavior", "Pop");
	popwhendone->setValue("MethodName", "checkDone");
	popwhendone->setState("");
	popwhendone->Init();

	mTransitionList["waittransition"] = waittransition;
	mTransitionList["popwhendone"] = popwhendone;

	// this one is needed for all cases
	fsm->addState("GetUserListDetail", new CoreFSMStateClass(TwitterAnalyser, GetUserListDetail)());
	// only wait or pop
	fsm->getState("GetUserListDetail")->addTransition(waittransition);

	// go to GetUserListDetail state (push)
	SP<CoreFSMTransition> userlistdetailtransition = KigsCore::GetInstanceOf("userlistdetailtransition", "CoreFSMOnValueTransition");
	userlistdetailtransition->setValue("TransitionBehavior", "Push");
	userlistdetailtransition->setValue("ValueName", "NeedUserListDetail");
	userlistdetailtransition->setState("GetUserListDetail");
	userlistdetailtransition->Init();

	mTransitionList["userlistdetailtransition"] = userlistdetailtransition;

	// this one is needed for all cases
	fsm->addState("Done", new CoreFSMStateClass(TwitterAnalyser, Done)());
	// only wait or pop
	fsm->getState("Done")->addTransition(userlistdetailtransition);

	// pop wait state transition
	SP<CoreFSMTransition> waitendtransition = KigsCore::GetInstanceOf("waitendtransition", "CoreFSMOnValueTransition");
	waitendtransition->setValue("TransitionBehavior", "Pop");
	waitendtransition->setValue("ValueName", "NeedWait");
	waitendtransition->setValue("NotValue", true); // end wait when NeedWait is false
	waitendtransition->Init();

	mTransitionList["waitendtransition"] = waitendtransition;

	// create Wait state
	fsm->addState("Wait", new CoreFSMStateClass(TwitterAnalyser, Wait)());
	// Wait state can pop back to previous state
	fsm->getState("Wait")->addTransition(waitendtransition);

	// transition to done state when checkDone returns true
	SP<CoreFSMTransition> donetransition = KigsCore::GetInstanceOf("donetransition", "CoreFSMOnMethodTransition");
	donetransition->setState("Done");
	donetransition->setValue("MethodName", "checkDone");
	donetransition->Init();

	mTransitionList["donetransition"] = donetransition;
}

std::string	TwitterAnalyser::searchPostersFSM()
{
	SP<CoreFSM> fsm = mFsm;

	fsm->addState("Init", new CoreFSMStateClass(TwitterAnalyser, InitHashTag)());

	// go to RetrieveTweetActors
	SP<CoreFSMTransition> retrievetweetactorstransition = KigsCore::GetInstanceOf("retrievetweetactorstransition", "CoreFSMInternalSetTransition");
	retrievetweetactorstransition->setState("RetrieveTweetActors");
	retrievetweetactorstransition->Init();

	// Init can go to Wait or GetTweets
	fsm->getState("Init")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("Init")->addTransition(retrievetweetactorstransition);

	auto retrievetweetactorstate = new CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)();
	// init state
	retrievetweetactorstate->mActorType = "Posters";
	retrievetweetactorstate->mMaxActorPerTweet = 1;

	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);


	// transition to GetRetweeters (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetPosters");
	getactorstransition->Init();

	// transition to GetTweets (push)
	SP<CoreFSMTransition> gettweetstransition = KigsCore::GetInstanceOf("gettweetstransition", "CoreFSMInternalSetTransition");
	gettweetstransition->setValue("TransitionBehavior", "Push");
	gettweetstransition->setState("GetTweets");
	gettweetstransition->Init();

	SP<CoreFSMTransition> retrievetweetstransition = KigsCore::GetInstanceOf("retrievetweetstransition", "CoreFSMInternalSetTransition");
	retrievetweetstransition->setValue("TransitionBehavior", "Push");
	retrievetweetstransition->setState("RetrieveTweets");
	retrievetweetstransition->Init();

	KigsCore::Connect(retrievetweetstransition.get(), "ExecuteTransition", this, "setRetrieveTweetsParams", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto retrievetweetsState = getFSMState(fsm, TwitterAnalyser, RetrieveTweets);
			retrievetweetsState->mUseHashtag = mUseHashTags;
			retrievetweetsState->mUserID = mPanelRetreivedUsers.getUserStructAtIndex(0).mID;
			retrievetweetsState->mUserName = mPanelRetreivedUsers.getUserStructAtIndex(0).mName.ToString();
			retrievetweetsState->mExcludeRetweets = true; // don't take retweets into account here
			retrievetweetsState->mExcludeReplies = true; // don't take replies into account here
		});
	fsm->addState("RetrieveTweets", new CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)());
	fsm->getState("RetrieveTweets")->addTransition(gettweetstransition);

	// can go to retrieve tweets, get actors or done (or manage actors in analyse block)
	fsm->getState("RetrieveTweetActors")->addTransition(getactorstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(retrievetweetstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["donetransition"]);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["userlistdetailtransition"]);

	// create GetRetweetd state

	auto getactorsstate = new CoreFSMStateClass(TwitterAnalyser, GetPosters)();
	getactorsstate->mNeededUserCount = 0; // get them all

	// GetRetweeted can just pop
	fsm->addState("GetPosters", getactorsstate);

	auto gettweetsstate = new CoreFSMStateClass(TwitterAnalyser, GetTweets)();
	// create GetTweets state
	fsm->addState("GetTweets", gettweetsstate);
	// after GetTweets, can wait or pop
	fsm->getState("GetTweets")->addTransition(mTransitionList["waittransition"]);

	return "RetrieveTweetActors";
}

void	TwitterAnalyser::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<FilePathManager> pathManager = KigsCore::Singleton<FilePathManager>();
	pathManager->AddToPath(".", "xml");
	// when a path is given, search the file only with this path
	pathManager->setValue("StrictPath", true);
	pathManager->AddToPath(".", "json");

	CoreCreateModule(HTTPRequestModule, 0);

	// Init App
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary("launchParams.json");

	// generic twitter management class
	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), TwitterConnect, TwitterConnect, TwitterAnalyser);
	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), ScrapperManager, ScrapperManager, TwitterAnalyser);
	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), GraphDrawer, GraphDrawer, TwitterAnalyser);

	mTwitterConnect = KigsCore::GetInstanceOf("mTwitterConnect", "TwitterConnect");
	mTwitterConnect->initBearer(initP);

	mGraphDrawer = KigsCore::GetInstanceOf("mGraphDrawer", "GraphDrawer");

	auto SetMemberFromParam = [&]<typename T>(T& x, const char* id) {
		if (initP[id]) x = initP[id].value<T>();
	};

	int oldFileLimitInDays = 3 * 30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");
	
	// check the kind of configuration we need
	std::string hashtag;
	SetMemberFromParam(hashtag, "HashTag");
	if (hashtag.length())
	{
		mUseHashTags = true;
		mPanelRetreivedUsers.addUser(0);
		auto textureManager = KigsCore::Singleton<TextureFileManager>();
		mPanelRetreivedUsers.getUserStructAtIndex(0).mThumb.mTexture = textureManager->GetTexture("keyw.png");
		mPanelRetreivedUsers.getUserStructAtIndex(0).mName = hashtag;
	}
	else
	{
		// look for dates (only if not hashtag)

		std::string fromdate, todate;
		SetMemberFromParam(fromdate, "FromDate");
		SetMemberFromParam(todate, "ToDate");

		if (fromdate.length() && todate.length())
		{
			TwitterConnect::initDates(fromdate, todate);
		}

		// look for since_id and since id
		std::string sinceID,untilID;
		SetMemberFromParam(untilID, "UntilID");
		SetMemberFromParam(sinceID, "SinceID");

		if (untilID.length())
		{
			TwitterConnect::setUntilID(untilID);
		}
		if (sinceID.length())
		{
			TwitterConnect::setSinceID(sinceID);
		}

		mPanelRetreivedUsers.addUser(0);
		SetMemberFromParam(mPanelRetreivedUsers.getUserStructAtIndex(0).mName, "UserName");
	}


	SetMemberFromParam(mTopUseBigger, "TopUseBigger");

	u32 PanelType;
	u32 AnalysedType;
	SetMemberFromParam(PanelType, "PanelType");
	mPanelType = (dataType)PanelType;
	SetMemberFromParam(AnalysedType, "AnalysedType");
	mAnalysedType = (dataType)AnalysedType;

	SetMemberFromParam(mUserPanelSize, "UserPanelSize");
	SetMemberFromParam(mValidUserPercent, "ValidUserPercent");
	SetMemberFromParam(mWantedTotalPanelSize, "WantedTotalPanelSize");
	SetMemberFromParam(mMaxUserCount, "MaxUserCount");

	initCoreFSM();

	std::string lastState;

	// add FSM
	SP<CoreFSM> fsm = KigsCore::GetInstanceOf("fsm", "CoreFSM");
	// need to add fsm to the object to control
	addItem(fsm);
	mFsm = fsm;

	commonStatesFSM();

	SetMemberFromParam(mMaxUsersPerTweet, "MaxUsersPerTweet");

	switch (mPanelType)
	{
	case dataType::Likers:
		lastState = searchLikersFSM();
		break;
	case dataType::Posters:
		if(mUseHashTags)
			lastState = searchPostersFSM();
		break;
	case dataType::Followers:
		lastState = searchFollowFSM("followers");
		break;
	case dataType::Following:
		lastState = searchFollowFSM("following");
		break;
	case dataType::Favorites:
		lastState = searchFavoritesFSM();
		break;
	case dataType::RTted:
		lastState = searchRetweetedFSM();
		break;
	case dataType::RTters:
		lastState = searchRetweetersFSM();
		break;
	case dataType::Replyers:
		lastState = searchReplyersFSM();
		break;
	case dataType::Interactors:
		lastState = searchInteractorsFSM();
		break;
	}

	switch (mAnalysedType)
	{
	case dataType::Likers:
		analyseLikersFSM(lastState);
		break;
	case dataType::Favorites:
		analyseFavoritesFSM(lastState);
		break;
	case dataType::Followers:
		analyseFollowFSM(lastState, "followers");
		break;
	case dataType::Following:
		analyseFollowFSM(lastState, "following");
		break;
	case dataType::TOP:
		TopFSM(lastState);
		break;
	case dataType::RTted:
		analyseRetweetedFSM(lastState);
		break;
	case dataType::RTters:
		analyseRetweetersFSM(lastState);
		break;
	case dataType::Replyers:
		analyseReplyersFSM(lastState);
		break;
	case dataType::Interactors:
		analyseInteractorsFSM(lastState);
		break;
	}

	postFSMSetup();

	mTwitterConnect->initConnection(60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays);

	// connect done msg
	KigsCore::Connect(mTwitterConnect.get(), "done", this, "requestDone");

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();

}


// setup special cases
void	TwitterAnalyser::postFSMSetup()
{
	if ((mPanelType == TwitterAnalyser::dataType::Following) && (mAnalysedType == TwitterAnalyser::dataType::Interactors))
	{
		mUserPanelSize = mWantedTotalPanelSize;
	}
}


void	TwitterAnalyser::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();
}

void	TwitterAnalyser::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
}

void	TwitterAnalyser::initLogos()
{
	auto panelLogo = mMainInterface["panelLogo"];
	auto twitterLogo = panelLogo["twitterLogo"];

	switch (mPanelType)
	{
		case dataType::Likers:
		{
			twitterLogo("Dock") = v2f(0.6f, 0.5f);
			twitterLogo["placeHolder1"]("TextureName") = "BigHeart.png";
			twitterLogo["placeHolder2"]("IsHidden") = true;
		}
		break;
		
		case dataType::Favorites:
		{
			twitterLogo("Dock") = v2f(0.4f, 0.5f);
			twitterLogo["placeHolder1"]("IsHidden") = true;
			twitterLogo["placeHolder2"]("TextureName") = "BigHeart.png";
		}
		break; 

		case dataType::Posters: 
		{
			twitterLogo("Dock") = v2f(0.4f, 0.5f);
			twitterLogo["placeHolder1"]("IsHidden") = true;
			twitterLogo["placeHolder2"]("TextureName") = "Writer.png";
		}
		break;

		case dataType::Followers:
		{
			twitterLogo("Dock") = v2f(0.6f, 0.5f);
			twitterLogo["placeHolder1"]("TextureName") = "Follow.png";
			twitterLogo["placeHolder2"]("IsHidden") = true;
		}
		break;

		case dataType::Following:
		{
			twitterLogo("Dock") = v2f(0.4f, 0.5f);
			twitterLogo["placeHolder1"]("IsHidden") = true;
			twitterLogo["placeHolder2"]("TextureName") = "Follow.png";
		}
		break;

		case dataType::RTters:
		{
			twitterLogo("Dock") = v2f(0.6f, 0.5f);
			twitterLogo["placeHolder1"]("TextureName") = "Retweet.png";
			twitterLogo["placeHolder2"]("IsHidden") = true;
		}
		break;

		case dataType::RTted:
		{
			twitterLogo("Dock") = v2f(0.4f, 0.5f);
			twitterLogo["placeHolder1"]("IsHidden") = true;
			twitterLogo["placeHolder2"]("TextureName") = "Retweet.png";
		}
		break;

		case dataType::Replyers:
		{
			twitterLogo("Dock") = v2f(0.6f, 0.5f);
			twitterLogo["placeHolder1"]("TextureName") = "reply.png";
			twitterLogo["placeHolder2"]("IsHidden") = true;
		}
		break;

		case dataType::Interactors:
		{
			twitterLogo("Dock") = v2f(0.4f, 0.5f);
			twitterLogo["placeHolder1"]("IsHidden") = true;
			twitterLogo["placeHolder2"]("TextureName") = "Interactors.png";
		}
		break;
	}
	
	switch (mAnalysedType)
	{
		case dataType::Likers:
		{
			panelLogo["placeHolder3"]("TextureName") = "BigHeart.png";
			panelLogo["placeHolder4"]("IsHidden") = true;
		}
		break;

		case dataType::Favorites:
		{
			panelLogo["placeHolder3"]("IsHidden") = true;
			panelLogo["placeHolder4"]("TextureName") = "BigHeart.png";
		}
		break;

		case dataType::TOP:
		{
			panelLogo["placeHolder3"]("TextureName") = "Top.png";
			panelLogo["placeHolder4"]("IsHidden") = true;
		}
		break;

		case dataType::Followers:
		{
			panelLogo["placeHolder3"]("TextureName") = "Follow.png";
			panelLogo["placeHolder4"]("IsHidden") = true;
		}
		break;

		case dataType::Following:
		{
			panelLogo["placeHolder3"]("IsHidden") = true;
			panelLogo["placeHolder4"]("TextureName") = "Follow.png";
		}
		break;

		case dataType::RTters:
		{
			panelLogo["placeHolder3"]("TextureName") = "Retweet.png";
			panelLogo["placeHolder4"]("IsHidden") = true;
		}
		break;

		case dataType::RTted:
		{
			panelLogo["placeHolder3"]("IsHidden") = true;
			panelLogo["placeHolder4"]("TextureName") = "Retweet.png";
		}
		break;

		case dataType::Replyers:
		{
			panelLogo["placeHolder3"]("TextureName") = "reply.png";
			panelLogo["placeHolder4"]("IsHidden") = true;
		}
		break;

		case dataType::Interactors:
		{
			panelLogo["placeHolder3"]("IsHidden") = true;
			panelLogo["placeHolder4"]("TextureName") = "Interactors.png";
		}
		break;
	}
}


void	TwitterAnalyser::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;

		initLogos();

		// launch fsm
		SP<CoreFSM> fsm = mFsm;
		fsm->setStartState("Init");
		fsm->Init();

		mGraphDrawer->setInterface(mMainInterface);
		mGraphDrawer->Init();
	}
}
void	TwitterAnalyser::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mGraphDrawer = nullptr;
		SP<CoreFSM> fsm = mFsm;
		mFsm = nullptr;
		removeItem(fsm);
		mMainInterface = nullptr;
		mPanelRetreivedUsers.clear();
	}
}

void	TwitterAnalyser::requestDone()
{
	mNeedWait = false;
}

void TwitterAnalyser::mainUserDone(TwitterConnect::UserStruct& CurrentUserStruct)
{
	// save user
	JSonFileParser L_JsonParser;
	CoreItemSP initP = MakeCoreMap();
	initP->set("id", CurrentUserStruct.mID);
	std::string filename = "Cache/UserName/";
	filename += CurrentUserStruct.mName.ToString() + ".json";
	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);

	manageRetrievedUserDetail(CurrentUserStruct);
}

void	TwitterAnalyser::manageRetrievedUserDetail(TwitterConnect::UserStruct& CurrentUserStruct)
{
	{
		// save name to id if needed
		std::string username = CurrentUserStruct.mName.ToString();
		if (username != "Unknown")
		{
			std::string filename = "Cache/Tweets/" + username.substr(0, 4) + "/" + username + ".json";
			// user id doesn't expire
			CoreItemSP currentUserJson = TwitterConnect::LoadJSon(filename, false);
			if (!currentUserJson)
			{
				JSonFileParser L_JsonParser;
				CoreItemSP initP = MakeCoreMap();
				initP->set("id", CurrentUserStruct.mID);
				L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
			}
		}

	}
	TwitterConnect::SaveUserStruct(CurrentUserStruct.mID, CurrentUserStruct);

	KigsCore::Disconnect(mTwitterConnect.get(), "UserDetailRetrieved", this, "manageRetrievedUserDetail");

	requestDone();
}


void	TwitterAnalyser::switchDisplay()
{
	mMainInterface["switchForce"]("IsHidden") = true;
	mMainInterface["switchForce"]("IsTouchable") = false;

	mGraphDrawer->nextDrawType();
}

void	TwitterAnalyser::switchForce()
{
	bool currentDrawForceState=mGraphDrawer->getValue<bool>("DrawForce");
	if (!currentDrawForceState)
	{
		mMainInterface["thumbnail"]("Dock") = v2f(0.94f, 0.08f);

		mMainInterface["switchV"]("IsHidden") = true;
		mMainInterface["switchV"]("IsTouchable") = false;

		mMainInterface["switchForce"]("Dock") = v2f(0.050f, 0.950f);

	}
	else
	{
		mMainInterface["thumbnail"]("Dock") = v2f(0.52f, 0.44f);

		mMainInterface["switchV"]("IsHidden") = false;
		mMainInterface["switchV"]("IsTouchable") = true;

		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;
		mMainInterface["switchForce"]("Dock") = v2f(0.950f, 0.050f);
	}
	currentDrawForceState = !currentDrawForceState;
	mGraphDrawer->setValue("DrawForce", currentDrawForceState);

}


bool	TwitterAnalyser::checkDone()
{
	return (mValidUserCount == mUserPanelSize);
}

void		TwitterAnalyser::SaveStatFile()
{

	std::string filename = mPanelRetreivedUsers.getUserStructAtIndex(0).mName.ToString();
	filename += "_stats.csv";

	// avoid slash and backslash
	std::replace(filename.begin(), filename.end(), '/', ' ');
	std::replace(filename.begin(), filename.end(), '\\', ' ');

	filename = FilePathManager::MakeValidFileName(filename);


	float wantedpercent = mValidUserPercent;

	std::vector<std::tuple<unsigned int, float, u64>>	toSave;
	for (auto c : mInStatsUsers)
	{
		if (c.first != mPanelRetreivedUsers.getUserStructAtIndex(0).mID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mValidUserCount;
				if (percent > wantedpercent)
				{
					toSave.push_back({ c.second.first,percent * 100.0f,c.first });
				}
			}
		}
	}

	std::sort(toSave.begin(), toSave.end(), [&](const std::tuple<unsigned int, float, u64>& a1, const std::tuple<unsigned int, float, u64>& a2)
		{
			if (std::get<0>(a1) == std::get<0>(a2))
			{
				return std::get<2>(a1) > std::get<2>(a2);
			}
			return (std::get<0>(a1) > std::get<0>(a2));
		}
	);
	
	SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
	if (L_File->mFile)
	{
		// save general stats
		char saveBuffer[4096];
	
		std::string header = "Account Name,Percent,Normalized\n";
		Platform_fwrite(header.c_str(), 1, header.length(), L_File.get());

		for (const auto& p : toSave)
		{
			auto& U = mInStatsUsers[std::get<2>(p)].second;

			float N= (U.mFollowersCount < 10) ? logf(10.0f) : logf((float)U.mFollowersCount);

			N = std::get<1>(p) / N;

			sprintf(saveBuffer, "\"%s\",%d,%d\n", U.mName.ToString().c_str(), (int)std::get<1>(p),  (int)(100.0f*N));
			Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		}


		Platform_fclose(L_File.get());
	}
}