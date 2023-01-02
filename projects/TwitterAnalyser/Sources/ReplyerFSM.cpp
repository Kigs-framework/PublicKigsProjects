#include "TwitterAnalyser.h"
#include "CoreFSMState.h"
#include "CoreFSM.h"
#include "CommonTwitterFSMStates.h"


using namespace Kigs;
using namespace Kigs::Fsm;
using namespace Kigs::File;

// specific states for replyers

std::string TwitterAnalyser::searchReplyersFSM()
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
	retrievetweetactorstate->mActorType = "Replyers";
	retrievetweetactorstate->mMaxActorPerTweet = mMaxUsersPerTweet;
	retrievetweetactorstate->mExcludeMainUser = true;

	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);


	// transition to GetReplyers (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetReplyers");
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

	// create GetReplyers state

	auto getreplyersstate = new CoreFSMStateClass(TwitterAnalyser, GetReplyers)();
	getreplyersstate->mNeededUserCount = 0; // get them all

	fsm->addState("GetReplyers", getreplyersstate);
	// after GetReplyers, can wait or pop
	fsm->getState("GetReplyers")->addTransition(mTransitionList["waittransition"]);

	auto gettweetsstate = new CoreFSMStateClass(TwitterAnalyser, GetTweets)();
	// create GetTweets state
	fsm->addState("GetTweets", gettweetsstate);
	// after GetTweets, can wait or pop
	fsm->getState("GetTweets")->addTransition(mTransitionList["waittransition"]);

	return "RetrieveTweetActors";
}



void	TwitterAnalyser::analyseReplyersFSM(const std::string& lastState)
{
	SP<CoreFSM> fsm = mFsm;

	// create new fsm block
	fsm->setCurrentBlock(fsm->addBlock());

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
	retrievetweetactorstate->mActorType = "Replyers";
	retrievetweetactorstate->mMaxActorPerTweet = mMaxUsersPerTweet;
	retrievetweetactorstate->mTreatAllActorsTogether = true;
	retrievetweetactorstate->mWantedActorCount = 200;
	retrievetweetactorstate->mExcludeMainUser = true;

	fsm->addState("RetrieveTweetActors", retrievetweetactorstate);

	// transition to GetReplyers (push)
	SP<CoreFSMTransition> getactorstransition = KigsCore::GetInstanceOf("getactorstransition", "CoreFSMInternalSetTransition");
	getactorstransition->setValue("TransitionBehavior", "Push");
	getactorstransition->setState("GetReplyers", 1);
	getactorstransition->Init();

	// transition to GetTweets (push)
	SP<CoreFSMTransition> gettweetstransition = KigsCore::GetInstanceOf("gettweetstransition", "CoreFSMInternalSetTransition");
	gettweetstransition->setValue("TransitionBehavior", "Push");
	gettweetstransition->setState("GetTweets", 1);
	gettweetstransition->Init();

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

			if (name == "")
			{
				printf("problem here");
			}
			retrievetweetsState->mUserID = user;
			retrievetweetsState->mUserName = name;
			retrievetweetsState->mExcludeRetweets = true; // don't take retweets into account here
			retrievetweetsState->mExcludeReplies = true; // don't take replies into account here
		});
	fsm->addState("RetrieveTweets", new CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)());
	fsm->getState("RetrieveTweets")->addTransition(gettweetstransition);

	// can go to retrieve tweets, get actors or done (or manage actors in analyse block)
	fsm->getState("RetrieveTweetActors")->addTransition(getactorstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(retrievetweetstransition);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["popwhendone"]);
	fsm->getState("RetrieveTweetActors")->addTransition(mTransitionList["userlistdetailtransition"]);

	// create GetReplyers state

	auto getreplyersstate = new CoreFSMStateClass(TwitterAnalyser, GetReplyers)();
	getreplyersstate->mNeededUserCount = 0; // get them all

	fsm->addState("GetReplyers", getreplyersstate);
	// after GetReplyers, can wait or pop
	fsm->getState("GetReplyers")->addTransition(mTransitionList["waittransition"]);

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

	fsm->getState("RetrieveTweetActors", 1)->addTransition(getuserdatatransition2);

	// create UpdateLikesStats state
	fsm->addState("UpdateStats", new CoreFSMStateClass(TwitterAnalyser, UpdateStats)());
	// no transition here, only pop


}

