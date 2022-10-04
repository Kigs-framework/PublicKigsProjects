#include "TwitterAnalyser.h"
#include "CoreFSMState.h"
#include "CoreFSM.h"
#include "CommonTwitterFSMStates.h"

START_INHERITED_COREFSMSTATE(TwitterAnalyser, GetInteractors, GetUsers)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()


START_INHERITED_COREFSMSTATE(TwitterAnalyser, GetUserAllInteractors, GetUsers) // get rtweeted + replyed first, then favorites
public:
	unsigned int mState = 0;
	bool		 mPopAtEachCycle=false;
STARTCOREFSMSTATE_WRAPMETHODS();
	void	copyUserList(TwitterAnalyser::UserList& touserlist);
ENDCOREFSMSTATE_WRAPMETHODS(copyUserList)
END_DECLARE_COREFSMSTATE()

// specific states for interactors

std::string TwitterAnalyser::searchInteractorsFSM()
{
	SP<CoreFSM> fsm = mFsm;

	// Init state, no hashtag here
	fsm->addState("Init", new CoreFSMStateClass(TwitterAnalyser, InitUser)());
	

	// create getFavorites transition (Push)
	SP<CoreFSMTransition> getfavoritestransition = KigsCore::GetInstanceOf("getfavoritestransition", "CoreFSMInternalSetTransition");
	getfavoritestransition->setValue("TransitionBehavior", "Push");
	getfavoritestransition->setState("GetFavorites");
	getfavoritestransition->Init();

	KigsCore::Connect(getfavoritestransition.get(), "ExecuteTransition", this, "getfavorites", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto userfavs = ((CoreFSMStateClass(TwitterAnalyser, GetFavorites)*)fsm->getState("GetFavorites"));

			userfavs->mUserID = mPanelRetreivedUsers.getUserStructAtIndex(0).mID;
			userfavs->mFavorites.clear();
			userfavs->mFavoritesCount = (mUserPanelSize * 150)/500;

		});

	// go to RetrieveTweetActors
	SP<CoreFSMTransition> retrievetweetactorstransition = KigsCore::GetInstanceOf("retrievetweetactorstransition", "CoreFSMInternalSetTransition");
	retrievetweetactorstransition->setValue("TransitionBehavior", "Push");
	retrievetweetactorstransition->setState("RetrieveTweetActors");
	retrievetweetactorstransition->Init();

	// go to GetUserAllInteractors
	SP<CoreFSMTransition> getuserallinteractorstransition = KigsCore::GetInstanceOf("getuserallinteractorstransition", "CoreFSMInternalSetTransition");
	getuserallinteractorstransition->setState("GetUserAllInteractors");
	getuserallinteractorstransition->Init();

	fsm->addState("GetUserAllInteractors", new CoreFSMStateClass(TwitterAnalyser, GetUserAllInteractors)());
	fsm->getState("GetUserAllInteractors")->addTransition(getfavoritestransition);
	fsm->getState("GetUserAllInteractors")->addTransition(retrievetweetactorstransition);
	fsm->getState("GetUserAllInteractors")->addTransition(mTransitionList["donetransition"]);

	// Init can go to Wait or GetTweets
	fsm->getState("Init")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("Init")->addTransition(getuserallinteractorstransition);

	auto retrievetweetactorstate = new CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)();
	// init state
	retrievetweetactorstate->mActorType = "Interactors"; // replied + retweeted
	retrievetweetactorstate->mMaxActorPerTweet = mMaxUsersPerTweet;
	retrievetweetactorstate->mExcludeMainUser = true;
	retrievetweetactorstate->mWantedActorCount = (mUserPanelSize * 350) / 500; // 350 + rtweeted or replied + 150 favorites  

	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);

	// once tweets where found
	// transition to GetInteractors (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetInteractors");
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
			retrievetweetsState->mExcludeRetweets = false; // all tweets
			retrievetweetsState->mExcludeReplies = false; // all tweets
		});
	fsm->addState("RetrieveTweets", new CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)());
	fsm->getState("RetrieveTweets")->addTransition(gettweetstransition);

	// can go to retrieve tweets, get actors or done (or manage actors in analyse block)
	fsm->getState("RetrieveTweetActors")->addTransition(getactorstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(retrievetweetstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["userlistdetailtransition"]);

	// create GetInteractors state

	auto getinteractorsstate = new CoreFSMStateClass(TwitterAnalyser, GetInteractors)();
	getinteractorsstate->mNeededUserCount = 0; // get them all

	fsm->addState("GetInteractors", getinteractorsstate);
	// after GetInteractors, can wait or pop
	fsm->getState("GetInteractors")->addTransition(mTransitionList["waittransition"]);

	auto gettweetsstate = new CoreFSMStateClass(TwitterAnalyser, GetTweets)();
	// create GetTweets state
	fsm->addState("GetTweets", gettweetsstate);
	// after GetTweets, can wait or pop
	fsm->getState("GetTweets")->addTransition(mTransitionList["waittransition"]);
	
	// create GetFavorites state
	fsm->addState("GetFavorites", new CoreFSMStateClass(TwitterAnalyser, GetFavorites)());
	// after GetFavorites, can go to get user data (or pop)
	fsm->getState("GetFavorites")->addTransition(mTransitionList["waittransition"]);

	return "GetUserAllInteractors";
}



void	TwitterAnalyser::analyseInteractorsFSM(const std::string& lastState)
{
	SP<CoreFSM> fsm = mFsm;

	// create new fsm block
	fsm->setCurrentBlock(fsm->addBlock());

	// create getFavorites transition (Push)
	SP<CoreFSMTransition> getfavoritestransition = KigsCore::GetInstanceOf("getfavoritestransition", "CoreFSMInternalSetTransition");
	getfavoritestransition->setValue("TransitionBehavior", "Push");
	getfavoritestransition->setState("GetFavorites",1);
	getfavoritestransition->Init();

	KigsCore::Connect(getfavoritestransition.get(), "ExecuteTransition", this, "getfavorites1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto userfavs = ((CoreFSMStateClass(TwitterAnalyser, GetFavorites)*)fsm->getState("GetFavorites"));

			auto userinteractors = ((CoreFSMStateClass(TwitterAnalyser, GetUserAllInteractors)*)fsm->getState("GetUserAllInteractors"));

			userfavs->mUserID = mPanelUserList.getList()[mCurrentTreatedPanelUserIndex].first;
			userfavs->mFavorites.clear();

			// retrieve favorites according to retrieved interactors count so far (2/3 vs 1/3)
			userfavs->mFavoritesCount = userinteractors->mUserlist.size()/2;

		});


	// generic get user data transition
	SP<CoreFSMTransition> getuserdatatransition = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition->setValue("TransitionBehavior", "PushBlock");
	getuserdatatransition->setState("GetUserDetail", 1);
	getuserdatatransition->Init();


	// retreive last state in block 0
	fsm->getState(lastState, 0)->addTransition(getuserdatatransition);

	KigsCore::Connect(getuserdatatransition.get(), "ExecuteTransition", this, "setuserdetailBlock1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			SimpleCall("copyUserList", mPanelUserList);

			auto userDetailState = fsm->getState("GetUserDetail", 1)->as<CoreFSMStateClass(TwitterAnalyser, GetUserDetail)>();

			u64 user = mPanelUserList.getList()[mCurrentTreatedPanelUserIndex].first;

			mPanelRetreivedUsers.addUser(user);
			userDetailState->mUserID = user;
			userDetailState->nextTransition = "getuserallinteractorstransition";

		});

	// create UpdateStats transition (Push)
	SP<CoreFSMTransition> retrievetweetactorstransition = KigsCore::GetInstanceOf("retrievetweetactorstransition", "CoreFSMInternalSetTransition");
	retrievetweetactorstransition->setValue("TransitionBehavior", "Push");
	retrievetweetactorstransition->setState("RetrieveTweetActors", 1);
	retrievetweetactorstransition->Init();

	KigsCore::Connect(retrievetweetactorstransition.get(), "ExecuteTransition", this, "setRetrieveTweetActorsParams1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto retrievetweetActors = fsm->getState("RetrieveTweetActors", 1)->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)>();
			retrievetweetActors->mUserlist.clear();
		});

	// go to GetUserAllInteractors
	SP<CoreFSMTransition> getuserallinteractorstransition = KigsCore::GetInstanceOf("getuserallinteractorstransition", "CoreFSMInternalSetTransition");
	getuserallinteractorstransition->setValue("TransitionBehavior", "Push");
	getuserallinteractorstransition->setState("GetUserAllInteractors", 1);
	getuserallinteractorstransition->Init();


	fsm->addState("GetUserDetail", new CoreFSMStateClass(TwitterAnalyser, GetUserDetail)());
	// GetUserDetail can go to RetrieveTweetActors, wait (or pop)
	fsm->getState("GetUserDetail")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("GetUserDetail")->addTransition(getuserallinteractorstransition);

	auto retrievetweetactorstate = new CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)();
	// init state
	retrievetweetactorstate->mActorType = "Interactors";
	retrievetweetactorstate->mMaxActorPerTweet = 1;
	retrievetweetactorstate->mTreatAllActorsTogether = true;
	retrievetweetactorstate->mWantedActorCount = 65; // 65 rtweeted + replied + 35 favorites


	fsm->addState("GetUserAllInteractors", new CoreFSMStateClass(TwitterAnalyser, GetUserAllInteractors)());
	fsm->getState("GetUserAllInteractors")->addTransition(getfavoritestransition);
	fsm->getState("GetUserAllInteractors")->addTransition(retrievetweetactorstransition);
	fsm->getState("GetUserAllInteractors")->addTransition(mTransitionList["popwhendone"]);

	{
		auto interactorsState = fsm->getState("GetUserAllInteractors", 1)->as<CoreFSMStateClass(TwitterAnalyser, GetUserAllInteractors)>();
		interactorsState->mPopAtEachCycle = true;
	}
	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);

	// transition to GetRetweeters (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetInteractors", 1);
	getactorstransition->Init();

	// transition to GetTweets (push)
	SP<CoreFSMTransition> gettweetstransition = KigsCore::GetInstanceOf("gettweetstransition", "CoreFSMInternalSetTransition");
	gettweetstransition->setValue("TransitionBehavior", "Push");
	gettweetstransition->setState("GetTweets", 1);
	gettweetstransition->Init();

	KigsCore::Connect(gettweetstransition.get(), "ExecuteTransition", this, "setgettweetsParams1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto gettweets = fsm->getState("GetTweets", 1)->as<CoreFSMStateClass(TwitterAnalyser, GetTweets)>();
			gettweets->mNeededTweetCount=0; // continue until no more needed
			gettweets->mTimeLimit = 60;
		});

	SP<CoreFSMTransition> retrievetweetstransition = KigsCore::GetInstanceOf("retrievetweetstransition", "CoreFSMInternalSetTransition");
	retrievetweetstransition->setValue("TransitionBehavior", "Push");
	retrievetweetstransition->setState("RetrieveTweets", 1);
	retrievetweetstransition->Init();

	KigsCore::Connect(retrievetweetstransition.get(), "ExecuteTransition", this, "setRetrieveTweetsParamsBlock1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto retrievetweetsState = fsm->getState("RetrieveTweets", 1)->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();
			retrievetweetsState->mUseHashtag = false;

			u64 user = mPanelUserList.getList()[mCurrentTreatedPanelUserIndex].first;
			std::string name = mPanelRetreivedUsers.getUserStruct(user).mName.ToString();
			retrievetweetsState->mUserID = user;
			retrievetweetsState->mUserName = name;
			retrievetweetsState->mExcludeRetweets = false; // take all
			retrievetweetsState->mExcludeReplies = false; // take all
			retrievetweetsState->mTimeLimit = 60;	// limit to 60 days

		});
	fsm->addState("RetrieveTweets", new CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)());
	fsm->getState("RetrieveTweets")->addTransition(gettweetstransition);

	// can go to retrieve tweets, get actors or done (or manage actors in analyse block)
	fsm->getState("RetrieveTweetActors")->addTransition(getactorstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(retrievetweetstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["userlistdetailtransition"]);


	auto getactorssstate = new CoreFSMStateClass(TwitterAnalyser, GetInteractors)();
	getactorssstate->mNeededUserCount = 0; // get them all

	fsm->addState("GetInteractors", getactorssstate);
	// after GetRetweeters, can wait or pop
	fsm->getState("GetInteractors")->addTransition(mTransitionList["waittransition"]);

	auto gettweetsstate = new CoreFSMStateClass(TwitterAnalyser, GetTweets)();
	// create GetTweets state
	fsm->addState("GetTweets", gettweetsstate);
	// after GetTweets, can wait or pop
	fsm->getState("GetTweets")->addTransition(mTransitionList["waittransition"]);


	//////////////////////////////////////////////////


	SP<CoreFSMTransition> getuserdatatransition2 = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition2->setValue("TransitionBehavior", "Push");
	getuserdatatransition2->setState("UpdateStats", 1);
	getuserdatatransition2->Init();

	KigsCore::Connect(getuserdatatransition2.get(), "ExecuteTransition", this, "setupdatedataBlock1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto interactorsState = fsm->getState("GetUserAllInteractors", 1)->as<CoreFSMStateClass(TwitterAnalyser, GetUserAllInteractors)>();
			auto updateState = fsm->getState("UpdateStats", 1)->as<CoreFSMStateClass(TwitterAnalyser, UpdateStats)>();
			if (interactorsState->mUserlist.size())
			{
				updateState->mUserlist = std::move(interactorsState->mUserlist);
				updateState->mIsValid = true;
			}
			else
			{
				updateState->mUserlist.clear();
				updateState->mIsValid = false;
			}
		});

	fsm->getState("GetUserAllInteractors", 1)->addTransition(getuserdatatransition2);

	// create GetFavorites state
	fsm->addState("GetFavorites", new CoreFSMStateClass(TwitterAnalyser, GetFavorites)());
	// after GetFavorites, can go to get user data (or pop)
	fsm->getState("GetFavorites")->addTransition(mTransitionList["waittransition"]);

	// create UpdateLikesStats state
	fsm->addState("UpdateStats", new CoreFSMStateClass(TwitterAnalyser, UpdateStats)());
	// no transition here, only pop
}

void	CoreFSMStartMethod(TwitterAnalyser, GetInteractors)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetInteractors)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetInteractors))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	SP<CoreFSM> fsm = mFsm;
	auto tweetsState = fsm->getState("RetrieveTweets")->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();

	GetUpgrador()->mUserlist.clear();

	if (GetUpgrador()->mForID != tweetsState->mUserID) // user id is != current tweet authorID ? this is a retweet or reply
	{
		GetUpgrador()->mUserlist.addUser(GetUpgrador()->mForID);
	}

	// the tweet is a reply or retweet so can't get more users here
	GetUpgrador()->mCantGetMoreUsers = true;

	GetUpgrador()->popState();

	return false;
}

void	CoreFSMStartMethod(TwitterAnalyser, GetUserAllInteractors)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetUserAllInteractors)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetUserAllInteractors))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	SP<CoreFSM> fsm = mFsm;
	
	if (GetUpgrador()->mState == 0)
	{
		GetUpgrador()->activateTransition("retrievetweetactorstransition");
		GetUpgrador()->mState = 1;
	}
	else if (GetUpgrador()->mState == 1)
	{
		// get users from retrievedtweetactors
		auto tweetactorState = fsm->getState("RetrieveTweetActors")->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)>();
		GetUpgrador()->mUserlist = tweetactorState->mUserlist;
	
		// retrieve favorites
		GetUpgrador()->activateTransition("getfavoritestransition");
		GetUpgrador()->mState = 2;	
	}
	else if (GetUpgrador()->mState == 2)
	{
		// get retrieved favorites
		auto favoritesState = fsm->getState("GetFavorites")->as<CoreFSMStateClass(TwitterAnalyser, GetFavorites)>();

		size_t count = 0;
		for (auto& twt : favoritesState->mFavorites)
		{
			if (GetUpgrador()->mUserlist.addUser(twt.mAuthorID))
			{
				count++;
				if (count > favoritesState->mFavoritesCount)
				{
					break;
				}
			}
		}
		GetUpgrador()->mState = 3;
	}
	else if (GetUpgrador()->mState == 3)
	{
		GetUpgrador()->activateTransition("getuserdatatransition");
		if(GetUpgrador()->mPopAtEachCycle)
			GetUpgrador()->mState = 4;
		else
			GetUpgrador()->mState = 0;
	}
	else if (GetUpgrador()->mState == 4)
	{
		GetUpgrador()->mState = 0;
		GetUpgrador()->popState();
	}

	return false;
}

void	CoreFSMStateClassMethods(TwitterAnalyser, GetUserAllInteractors)::copyUserList(TwitterAnalyser::UserList& touserlist)
{
	touserlist = GetUpgrador()->mUserlist;
}

