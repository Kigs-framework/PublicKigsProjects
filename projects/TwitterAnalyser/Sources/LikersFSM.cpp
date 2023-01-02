#include "TwitterAnalyser.h"
#include "CoreFSMState.h"
#include "CoreFSM.h"
#include "CommonTwitterFSMStates.h"


using namespace Kigs;
using namespace Kigs::Fsm;
using namespace Kigs::File;

// specific states for likers/favorites


START_DECLARE_COREFSMSTATE(TwitterAnalyser, RetrieveUserFavorites)
public:
	unsigned int					mStateStep = 0;
	TwitterAnalyser::UserList		mUserlist;
protected:
STARTCOREFSMSTATE_WRAPMETHODS();
void	copyUserList(TwitterAnalyser::UserList& touserlist);
ENDCOREFSMSTATE_WRAPMETHODS(copyUserList)
END_DECLARE_COREFSMSTATE()


// retrieve favorites
START_DECLARE_COREFSMSTATE(TwitterAnalyser, RetrieveFavorites)
public:
	unsigned int								mStateStep=0;
	std::vector<TwitterConnect::Twts>			mFavorites;
	u32											mValidTreatedUserForThisTweet=0;
protected:
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()



std::string TwitterAnalyser::searchLikersFSM()
{
	SP<CoreFSM> fsm = mFsm;

	// Init state, check if user was already started and launch next steps
	if (mUseHashTags)
	{
		fsm->addState("Init", new CoreFSMStateClass(TwitterAnalyser, InitHashTag)());
	}
	else
	{
		fsm->addState("Init", new CoreFSMStateClass(TwitterAnalyser, InitUser)());
	}

	// go to RetrieveTweetActors
	SP<CoreFSMTransition> retrievetweetactorstransition = KigsCore::GetInstanceOf("retrievetweetactorstransition", "CoreFSMInternalSetTransition");
	retrievetweetactorstransition->setState("RetrieveTweetActors");
	retrievetweetactorstransition->Init();

	// Init can go to Wait or GetTweets
	fsm->getState("Init")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("Init")->addTransition(retrievetweetactorstransition);

	auto retrievetweetactorstate = new CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)();
	// init state
	retrievetweetactorstate->mActorType = "Likers";
	retrievetweetactorstate->mMaxActorPerTweet = mMaxUsersPerTweet;

	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);


	// transition to GetLikers (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetLikers");
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
			retrievetweetsState->mExcludeReplies = false; // take replies into account here
		});
	fsm->addState("RetrieveTweets", new CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)());
	fsm->getState("RetrieveTweets")->addTransition(gettweetstransition);

	// can go to retrieve tweets, get actors or done (or manage actors in analyse block)
	fsm->getState("RetrieveTweetActors")->addTransition(getactorstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(retrievetweetstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["donetransition"]);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["userlistdetailtransition"]);

	// create GetLikers state

	auto getlikersstate = new CoreFSMStateClass(TwitterAnalyser, GetLikers)();
	getlikersstate->mNeededUserCount = 0; // get them all

	fsm->addState("GetLikers", getlikersstate);
	// after GetLikers, can wait or pop
	fsm->getState("GetLikers")->addTransition(mTransitionList["waittransition"]);

	auto gettweetsstate = new CoreFSMStateClass(TwitterAnalyser, GetTweets)();
	// create GetTweets state
	fsm->addState("GetTweets", gettweetsstate);
	// after GetTweets, can wait or pop
	fsm->getState("GetTweets")->addTransition(mTransitionList["waittransition"]);

	return "RetrieveTweetActors";
}

std::string TwitterAnalyser::searchFavoritesFSM()
{
	SP<CoreFSM> fsm = mFsm;

	// Init state, check if user was already started and launch next steps
	fsm->addState("Init", new CoreFSMStateClass(TwitterAnalyser, InitUser)());

	// go to RetrieveUserFavorites
	SP<CoreFSMTransition> retrieveuserfavoritestransition = KigsCore::GetInstanceOf("retrieveuserfavoritestransition", "CoreFSMInternalSetTransition");
	retrieveuserfavoritestransition->setState("RetrieveUserFavorites");
	retrieveuserfavoritestransition->Init();

	// create getFavorites transition (Push)
	SP<CoreFSMTransition> getfavoritestransition = KigsCore::GetInstanceOf("getfavoritestransition", "CoreFSMInternalSetTransition");
	getfavoritestransition->setValue("TransitionBehavior", "Push");
	getfavoritestransition->setState("GetFavorites");
	getfavoritestransition->Init();

	KigsCore::Connect(retrieveuserfavoritestransition.get(), "ExecuteTransition", this, "setUserID", [this,fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			
			mPanelUserList.addUser(mPanelRetreivedUsers.getUserStructAtIndex(0).mID);

			auto userfavs=((CoreFSMStateClass(TwitterAnalyser, GetFavorites)*)fsm->getState("GetFavorites"));

			userfavs->mUserID = mPanelRetreivedUsers.getUserStructAtIndex(0).mID;
			userfavs->mFavorites.clear();
			userfavs->mFavoritesCount = 1000;

		});

	// Init can go to Wait or GetTweets
	fsm->getState("Init")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("Init")->addTransition(retrieveuserfavoritestransition);

	// create RetrieveUserFavorites state
	fsm->addState("RetrieveUserFavorites", new CoreFSMStateClass(TwitterAnalyser, RetrieveUserFavorites)());
	fsm->getState("RetrieveUserFavorites")->addTransition(getfavoritestransition);
	fsm->getState("RetrieveUserFavorites")->addTransition(mTransitionList["donetransition"]);
	// RetrieveFavorites can also go to NeedUserListDetail
	fsm->getState("RetrieveUserFavorites")->addTransition(mTransitionList["userlistdetailtransition"]);
	

	// create GetFavorites state
	fsm->addState("GetFavorites", new CoreFSMStateClass(TwitterAnalyser, GetFavorites)());
	// after GetFavorites, can go to get user data (or pop)
	fsm->getState("GetFavorites")->addTransition(mTransitionList["waittransition"]);
	// GetFavorites can also go to NeedUserListDetail
	fsm->getState("GetFavorites")->addTransition(mTransitionList["userlistdetailtransition"]);
	
	return "RetrieveUserFavorites";
}


void	TwitterAnalyser::analyseFavoritesFSM(const std::string& lastState)
{
	SP<CoreFSM> fsm = mFsm;

	// create new fsm block
	fsm->setCurrentBlock(fsm->addBlock());

	// generic get user data transition
	SP<CoreFSMTransition> getuserdatatransition = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition->setValue("TransitionBehavior", "PushBlock");
	getuserdatatransition->setState("RetrieveFavorites",1); // change block index
	getuserdatatransition->Init();

	fsm->getState(lastState,0)->addTransition(getuserdatatransition);

	// when going to getuserdatatransition, retreive user list from previous state
	KigsCore::Connect(getuserdatatransition.get(), "ExecuteTransition", this, "setUserListblock1", [this](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			SimpleCall("copyUserList", mPanelUserList);
		});


	// create GetFavorites state
	fsm->addState("RetrieveFavorites", new CoreFSMStateClass(TwitterAnalyser, RetrieveFavorites)());

	// create getFavorites transition (Push)
	SP<CoreFSMTransition> getfavoritestransition = KigsCore::GetInstanceOf("getfavoritestransition", "CoreFSMInternalSetTransition");
	getfavoritestransition->setValue("TransitionBehavior", "Push");
	getfavoritestransition->setState("GetFavorites",1);
	getfavoritestransition->Init();

	// create GetUserDetail transition (Push)
	SP<CoreFSMTransition> getuserdetailtransition = KigsCore::GetInstanceOf("getuserdetailtransition", "CoreFSMInternalSetTransition");
	getuserdetailtransition->setValue("TransitionBehavior", "Push");
	getuserdetailtransition->setState("GetUserDetail",1);
	getuserdetailtransition->Init();

	// RetrieveFavorites -> get favorites / user detail, wait or pop
	fsm->getState("RetrieveFavorites")->addTransition(getfavoritestransition);
	fsm->getState("RetrieveFavorites")->addTransition(getuserdetailtransition);
	// RetrieveFavorites can also go to NeedUserListDetail
	fsm->getState("RetrieveFavorites")->addTransition(mTransitionList["userlistdetailtransition"]);
	fsm->getState("RetrieveFavorites")->addTransition(mTransitionList["popwhendone"]);

	fsm->addState("GetFavorites", new CoreFSMStateClass(TwitterAnalyser, GetFavorites)());

	// GetFavorites can go to waittransition or pop
	fsm->getState("GetFavorites")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("GetFavorites")->addTransition(mTransitionList["userlistdetailtransition"]);

	fsm->addState("GetUserDetail", new CoreFSMStateClass(TwitterAnalyser, GetUserDetail)());

	// create UpdateStats transition (Push)
	SP<CoreFSMTransition> updatestatstransition = KigsCore::GetInstanceOf("updatestatstransition", "CoreFSMInternalSetTransition");
	updatestatstransition->setValue("TransitionBehavior", "Push");
	updatestatstransition->setState("UpdateStats",1);
	updatestatstransition->Init();

	// GetUserDetail can go to UpdateLikesStats, wait (or pop)
	fsm->getState("GetUserDetail")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("GetUserDetail")->addTransition(updatestatstransition);

		// create UpdateStats state
	fsm->addState("UpdateStats", new CoreFSMStateClass(TwitterAnalyser, UpdateStats)());
	// no transition here, only pop


	KigsCore::Connect(updatestatstransition.get(), "ExecuteTransition", this, "setupstats", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			// get follow state
			auto favState = fsm->getState("RetrieveFavorites",1)->as<CoreFSMStateClass(TwitterAnalyser, RetrieveFavorites)>();
			auto updateState = fsm->getState("UpdateStats",1)->as<CoreFSMStateClass(TwitterAnalyser, UpdateStats)>();

			updateState->mUserlist.clear();

			for (auto& f : favState->mFavorites)
			{
				updateState->mUserlist.addUser(f.mAuthorID);
			}

		});
}


void	TwitterAnalyser::analyseLikersFSM(const std::string& lastState)
{
	SP<CoreFSM> fsm = mFsm;

	// create new fsm block
	fsm->setCurrentBlock(fsm->addBlock());

	// generic get user data transition
	SP<CoreFSMTransition> getuserdatatransition = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition->setValue("TransitionBehavior", "PushBlock");
	getuserdatatransition->setState("GetUserDetail",1);
	getuserdatatransition->Init();

	// retreive last state in block 0
	fsm->getState(lastState,0)->addTransition(getuserdatatransition);

	KigsCore::Connect(getuserdatatransition.get(), "ExecuteTransition", this, "setuserdetailBlock1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			SimpleCall("copyUserList", mPanelUserList);

			auto userDetailState = fsm->getState("GetUserDetail", 1)->as<CoreFSMStateClass(TwitterAnalyser, GetUserDetail)>();

			u64 user = mPanelUserList.getList()[mCurrentTreatedPanelUserIndex].first;

			mPanelRetreivedUsers.addUser(user);
			userDetailState->mUserID = user;
			userDetailState->nextTransition = "tweetactorstransition";
			
		});

	// create UpdateStats transition (Push)
	SP<CoreFSMTransition> tweetactorstransition = KigsCore::GetInstanceOf("tweetactorstransition", "CoreFSMInternalSetTransition");
	tweetactorstransition->setValue("TransitionBehavior", "Push");
	tweetactorstransition->setState("RetrieveTweetActors", 1);
	tweetactorstransition->Init();

	fsm->addState("GetUserDetail", new CoreFSMStateClass(TwitterAnalyser, GetUserDetail)());
	// GetUserDetail can go to RetrieveTweetActors, wait (or pop)
	fsm->getState("GetUserDetail")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("GetUserDetail")->addTransition(tweetactorstransition);

	auto retrievetweetactorstate = new CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)();
	// init state
	retrievetweetactorstate->mActorType = "Likers";
	retrievetweetactorstate->mMaxActorPerTweet = mMaxUsersPerTweet;
	retrievetweetactorstate->mTreatAllActorsTogether = true;
	retrievetweetactorstate->mWantedActorCount = 200;

	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);

	// transition to GetLikers (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetLikers",1);
	getactorstransition->Init();

	// transition to GetTweets (push)
	SP<CoreFSMTransition> gettweetstransition = KigsCore::GetInstanceOf("gettweetstransition", "CoreFSMInternalSetTransition");
	gettweetstransition->setValue("TransitionBehavior", "Push");
	gettweetstransition->setState("GetTweets",1);
	gettweetstransition->Init();

	SP<CoreFSMTransition> retrievetweetstransition = KigsCore::GetInstanceOf("retrievetweetstransition", "CoreFSMInternalSetTransition");
	retrievetweetstransition->setValue("TransitionBehavior", "Push");
	retrievetweetstransition->setState("RetrieveTweets",1);
	retrievetweetstransition->Init();

	KigsCore::Connect(retrievetweetstransition.get(), "ExecuteTransition", this, "setRetrieveTweetsParamsBlock1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto retrievetweetsState = fsm->getState("RetrieveTweets", 1)->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();
			retrievetweetsState->mUseHashtag = false;
			
			u64 user = mPanelUserList.getList()[mCurrentTreatedPanelUserIndex].first;
			std::string name=mPanelRetreivedUsers.getUserStruct(user).mName.ToString();
			
			if (name == "")
			{
				printf("problem here");
			}
			retrievetweetsState->mUserID = user;
			retrievetweetsState->mUserName = name;
			retrievetweetsState->mExcludeRetweets = true; // don't take retweets into account here
			retrievetweetsState->mExcludeReplies = false; // take replies into account here
		});
	fsm->addState("RetrieveTweets", new CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)());
	fsm->getState("RetrieveTweets")->addTransition(gettweetstransition);

	// can go to retrieve tweets, get actors or done (or manage actors in analyse block)
	fsm->getState("RetrieveTweetActors")->addTransition(getactorstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(retrievetweetstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["popwhendone"]);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["userlistdetailtransition"]);

	// create GetLikers state

	auto getlikersstate = new CoreFSMStateClass(TwitterAnalyser, GetLikers)();
	getlikersstate->mNeededUserCount = 0; // get them all

	fsm->addState("GetLikers", getlikersstate);
	// after GetLikers, can wait or pop
	fsm->getState("GetLikers")->addTransition(mTransitionList["waittransition"]);

	auto gettweetsstate = new CoreFSMStateClass(TwitterAnalyser, GetTweets)();
	// create GetTweets state
	fsm->addState("GetTweets", gettweetsstate);
	// after GetTweets, can wait or pop
	fsm->getState("GetTweets")->addTransition(mTransitionList["waittransition"]);


	//////////////////////////////////////////////////


	SP<CoreFSMTransition> getuserdatatransition2 = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition2->setValue("TransitionBehavior", "Push");
	getuserdatatransition2->setState("UpdateStats",1);
	getuserdatatransition2->Init();

	KigsCore::Connect(getuserdatatransition2.get(), "ExecuteTransition", this, "setupdatedataBlock1", [this, fsm](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			auto actorsState = fsm->getState("RetrieveTweetActors", 1)->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors)>();
			auto updateState = fsm->getState("UpdateStats", 1)->as<CoreFSMStateClass(TwitterAnalyser, UpdateStats)>();
			if (actorsState->mUserlist.size())
			{
				updateState->mUserlist = std::move(actorsState->mUserlist);
				updateState->mIsValid = true;
			}
			else
			{
				updateState->mUserlist.clear();
				updateState->mIsValid = false;
			}
		});

	fsm->getState("RetrieveTweetActors",1)->addTransition(getuserdatatransition2);

	// create UpdateLikesStats state
	fsm->addState("UpdateStats", new CoreFSMStateClass(TwitterAnalyser, UpdateStats)());
	// no transition here, only pop

		
}


void	CoreFSMStartMethod(TwitterAnalyser, RetrieveUserFavorites)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, RetrieveUserFavorites)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, RetrieveUserFavorites))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	if (GetUpgrador()->mStateStep == 0)
	{
		GetUpgrador()->mStateStep = 1;
		GetUpgrador()->activateTransition("getfavoritestransition");
	}
	else
	{
		SP<CoreFSM> fsm = mFsm;
		auto userfavs = ((CoreFSMStateClass(TwitterAnalyser, GetFavorites)*)fsm->getState("GetFavorites"));

		GetUpgrador()->mUserlist.clear();
		for (const auto& u : userfavs->mFavorites)
		{
			GetUpgrador()->mUserlist.addUser(u.mAuthorID);
		}

		GetUpgrador()->activateTransition("getuserdatatransition");

		GetUpgrador()->mStateStep = 0;
	}


	return false;
}

void	CoreFSMStateClassMethods(TwitterAnalyser, RetrieveUserFavorites)::copyUserList(TwitterAnalyser::UserList& touserlist)
{
	touserlist = std::move(GetUpgrador()->mUserlist);
}

void	CoreFSMStartMethod(TwitterAnalyser, RetrieveFavorites)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, RetrieveFavorites)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, RetrieveFavorites))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}
	auto userlist = mPanelUserList.getList();

	if (GetUpgrador()->mStateStep == 0)
	{
		if ((mCurrentTreatedPanelUserIndex < userlist.size()) && ((GetUpgrador()->mValidTreatedUserForThisTweet < mMaxUsersPerTweet) || (!mMaxUsersPerTweet)))
		{
			auto user = userlist[mCurrentTreatedPanelUserIndex].first;
			
			SP<CoreFSM> fsm = mFsm;
			auto getGetFavoritesState = getFSMState(fsm, TwitterAnalyser, GetFavorites);

			getGetFavoritesState->mUserID = user;
			getGetFavoritesState->mFavorites.clear();
			getGetFavoritesState->mFavoritesCount = 200;

			GetUpgrador()->activateTransition("getfavoritestransition");

			GetUpgrador()->mStateStep = 1;
		}
		else if (userlist.size())// treat next tweet
		{
			if (mCurrentTreatedPanelUserIndex < userlist.size())
			{
				mCanGetMoreUsers = true;
			}
			userlist.clear();
			GetUpgrador()->mValidTreatedUserForThisTweet = 0;
			GetUpgrador()->popState();
		}
	}
	else if(GetUpgrador()->mStateStep == 1)
	{
		auto user = userlist[mCurrentTreatedPanelUserIndex].first;
		SP<CoreFSM> fsm = mFsm;
		auto getGetFavoritesState = getFSMState(fsm, TwitterAnalyser, GetFavorites);

		GetUpgrador()->mFavorites = std::move(getGetFavoritesState->mFavorites);

		// if favorites were retrieved
		if(GetUpgrador()->mFavorites.size())
		{
			mPanelRetreivedUsers.addUser(user);
			
			SP<CoreFSM> fsm = mFsm;

			auto userDetailState = getFSMState(fsm, TwitterAnalyser, GetUserDetail);
			userDetailState->mUserID = user;
			userDetailState->nextTransition = "updatestatstransition";
			GetUpgrador()->activateTransition("getuserdetailtransition");
			
			GetUpgrador()->mStateStep = 0;
		}
		else
		{
			mCurrentTreatedPanelUserIndex++; // goto next one
			mTreatedUserCount++;
			GetUpgrador()->mStateStep = 0;
		}
	}
	
	
	return false;
}

