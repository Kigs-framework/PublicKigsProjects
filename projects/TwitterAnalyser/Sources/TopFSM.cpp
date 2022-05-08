#include "TwitterAnalyser.h"
#include "CoreFSMState.h"
#include "CoreFSM.h"
#include "CommonTwitterFSMStates.h"

START_DECLARE_COREFSMSTATE(TwitterAnalyser, UpdateTopStats)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()



void TwitterAnalyser::TopFSM(const std::string& laststate)
{
	SP<CoreFSM> fsm = mFsm;
	
	// generic get user data transition
	SP<CoreFSMTransition> getuserdatatransition = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition->setValue("TransitionBehavior", "Push");
	getuserdatatransition->setState("UpdateTopStats");
	getuserdatatransition->Init();

	fsm->getState(laststate)->addTransition(getuserdatatransition);

	// when going to getuserdatatransition, retreive user list from previous state
	KigsCore::Connect(getuserdatatransition.get(), "ExecuteTransition", this, "setUserList", [this](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			SimpleCall("copyUserList", mPanelUserList);
		});
	
	// create UpdateStats state
	fsm->addState("UpdateTopStats", new CoreFSMStateClass(TwitterAnalyser, UpdateTopStats)());

	// create GetUserDetail transition (Push)
	SP<CoreFSMTransition> getuserdetailtransition = KigsCore::GetInstanceOf("getuserdetailtransition", "CoreFSMInternalSetTransition");
	getuserdetailtransition->setValue("TransitionBehavior", "Push");
	getuserdetailtransition->setState("GetUserDetail");
	getuserdetailtransition->Init();

	// getFavorites -> user detail, wait or pop
	fsm->getState("UpdateTopStats")->addTransition(getuserdetailtransition);

	fsm->getState("UpdateTopStats")->addTransition(mTransitionList["waittransition"]);
	// GetFavorites can also go to NeedUserListDetail
	fsm->getState("UpdateTopStats")->addTransition(mTransitionList["userlistdetailtransition"]);
	fsm->getState("UpdateTopStats")->addTransition(mTransitionList["popwhendone"]);

	fsm->addState("GetUserDetail", new CoreFSMStateClass(TwitterAnalyser, GetUserDetail)());

	// GetUserDetail can only wait (or pop)
	fsm->getState("GetUserDetail")->addTransition(mTransitionList["waittransition"]);

}

void	CoreFSMStartMethod(TwitterAnalyser, UpdateTopStats)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, UpdateTopStats)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, UpdateTopStats))
{
	auto userlist = mPanelUserList.getList();
	
	switch (mPanelType)
	{
		case dataType::Likers:
		case dataType::Favorites:
		{
			for (auto u : userlist)
			{
				if (mInStatsUsers.find(u.first) == mInStatsUsers.end())
				{
					TwitterConnect::UserStruct	toAdd;
					mInStatsUsers[u.first] = std::pair<unsigned int, TwitterConnect::UserStruct>(1, toAdd);
				}
				else
				{
					mInStatsUsers[u.first].first++;
				}
			}

			mValidUserCount = mInStatsUsers.size();
			mUserPanelSize = mValidUserCount;
			userlist.clear();
			break;
		}

		case dataType::Followers:
		case dataType::Following:
		{
			if (mCurrentTreatedPanelUserIndex < userlist.size())
			{
				u64 user = userlist[mCurrentTreatedPanelUserIndex].first;

				TwitterConnect::UserStruct	newuser;
				if (!TwitterConnect::LoadUserStruct(user, newuser, false))
				{
					mPanelRetreivedUsers.addUser(user);
					
					SP<CoreFSM> fsm = mFsm;

					auto userDetailState = getFSMState(fsm, TwitterAnalyser, GetUserDetail);

					userDetailState->nextTransition = "Pop";
					userDetailState->mUserID = user;

					GetUpgrador()->activateTransition("getuserdetailtransition");
				}
				else
				{
					TwitterConnect::UserStruct	toAdd;
					mInStatsUsers[user] = std::pair<unsigned int, TwitterConnect::UserStruct>(newuser.mFollowersCount, toAdd);
					mCurrentTreatedPanelUserIndex++;
					mTreatedUserCount++;
					if(newuser.mFollowersCount)
						mValidUserCount++;
				}
			}
			else
			{
				mUserPanelSize = mValidUserCount;
			}
			break;

		}
	}

	return false;
}