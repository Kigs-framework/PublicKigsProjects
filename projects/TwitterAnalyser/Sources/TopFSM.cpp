#include "TwitterAnalyser.h"
#include "CoreFSMState.h"
#include "CoreFSM.h"
#include "CommonTwitterFSMStates.h"

START_DECLARE_COREFSMSTATE(TwitterAnalyser, UpdateTopStats)
public:
	u32	mMinCountForDetail = 2;
protected:
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()



void TwitterAnalyser::TopFSM(const std::string& laststate)
{
	SP<CoreFSM> fsm = mFsm;
	
	// generic get user data transition
	SP<CoreFSMTransition> getuserdatatransition = KigsCore::GetInstanceOf("getuserdatatransition", "CoreFSMInternalSetTransition");
	getuserdatatransition->setValue("TransitionBehavior", "Push");
	getuserdatatransition->setState("UpdateTopStats",1);
	getuserdatatransition->Init();

	fsm->getState(laststate,0)->addTransition(getuserdatatransition);

	// when going to getuserdatatransition, retreive user list from previous state
	KigsCore::Connect(getuserdatatransition.get(), "ExecuteTransition", this, "setUserListblock1", [this](CoreFSMTransition* t, CoreFSMStateBase* from)
		{
			SimpleCall("copyUserList", mPanelUserList);
		});
	
	// create UpdateStats state
	fsm->addState("UpdateTopStats", new CoreFSMStateClass(TwitterAnalyser, UpdateTopStats)());

	// create GetUserDetail transition (Push)
	SP<CoreFSMTransition> getuserdetailtransition = KigsCore::GetInstanceOf("getuserdetailtransition", "CoreFSMInternalSetTransition");
	getuserdetailtransition->setValue("TransitionBehavior", "Push");
	getuserdetailtransition->setState("GetUserDetail",1);
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

	switch (mPanelType)
	{
	case dataType::Likers:
	case dataType::RTters:
	case dataType::Replyers:
	{
		auto retrievetweetactorstate = getFSMState(fsm, TwitterAnalyser, RetrieveTweetActors);
		retrievetweetactorstate->mTreatFullList = true;
		mUserPanelSize /= 2;
	}
	break;
	}
}

void	CoreFSMStartMethod(TwitterAnalyser, UpdateTopStats)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, UpdateTopStats)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, UpdateTopStats))
{
	auto& userlist = mPanelUserList.getList();
	
	switch (mPanelType)
	{

	case dataType::Likers:
	case dataType::RTters:
	case dataType::Replyers:
	{
		while (mCurrentTreatedPanelUserIndex < userlist.size())
		{
			u64 user = userlist[mCurrentTreatedPanelUserIndex].first;

			auto& tstinit = mPanelRetreivedUsers.getUserStruct(user);
			bool alreadyInit = (tstinit.mFollowersCount + tstinit.mFollowingCount + tstinit.mName.length())>0;
			
			if ( (userlist[mCurrentTreatedPanelUserIndex].second > GetUpgrador()->mMinCountForDetail) && (!alreadyInit) && (!TwitterConnect::LoadUserStruct(user, tstinit, false)))
			{
				SP<CoreFSM> fsm = mFsm;

				auto userDetailState = getFSMState(fsm, TwitterAnalyser, GetUserDetail);

				userDetailState->nextTransition = "Pop";
				userDetailState->mUserID = user;


				GetUpgrador()->activateTransition("getuserdetailtransition");
				return false;
			}
			else
			{
				// just add user to panel user stats
				auto& currentUserData = mPerPanelUsersStats[user];
				mCurrentTreatedPanelUserIndex++;
			}
		}
		
		// update mMinCountForDetail
		u32 currentMinCountForDetail = GetUpgrador()->mMinCountForDetail;
		u32 countSuperiors = 0;
		for (auto& userinlist : userlist)
		{
			if (userinlist.second > currentMinCountForDetail)
			{
				countSuperiors++;
			}
		}

		if (countSuperiors > mUserPanelSize)
		{
			GetUpgrador()->mMinCountForDetail++;
		}

		// refresh all 
		for (auto i : userlist)
		{
			u64 refreshuser = i.first;
			mInStatsUsers[refreshuser] = std::pair<unsigned int, TwitterConnect::UserStruct>(i.second, mPanelRetreivedUsers.getUserStruct(refreshuser));
		}
		// per tweet management => divide per 2 the number of needed treated user
		mTreatedUserCount++;
		mValidUserCount++;
		mCurrentTreatedPanelUserIndex = 0;
		GetUpgrador()->popState();
		
		break;
	}

	case dataType::Favorites:

	{
		if (mCurrentTreatedPanelUserIndex < userlist.size())
		{
			u64 user = userlist[mCurrentTreatedPanelUserIndex].first;

			auto& tstinit = mPanelRetreivedUsers.getUserStruct(user);
			bool alreadyInit = (tstinit.mFollowersCount + tstinit.mFollowingCount + tstinit.mName.length()) > 0;

			if ( (!alreadyInit) && (!TwitterConnect::LoadUserStruct(user, tstinit, false)))
			{
				SP<CoreFSM> fsm = mFsm;

				auto userDetailState = getFSMState(fsm, TwitterAnalyser, GetUserDetail);

				userDetailState->nextTransition = "Pop";
				userDetailState->mUserID = user;


				GetUpgrador()->activateTransition("getuserdetailtransition");
			}
			else
			{
				// just add user to panel user stats
				auto& currentUserData = mPerPanelUsersStats[user];

				mInStatsUsers[user] = std::pair<unsigned int, TwitterConnect::UserStruct>(userlist[mCurrentTreatedPanelUserIndex].second, mPanelRetreivedUsers.getUserStruct(user));
					
				mCurrentTreatedPanelUserIndex++;
				mTreatedUserCount++;
				if (mPanelRetreivedUsers.getUserStruct(user).mFollowersCount || mPanelRetreivedUsers.getUserStruct(user).mFollowingCount)
					mValidUserCount++;

			}
		}
		else
		{
			mUserPanelSize = mValidUserCount;
		}
		break;
	}

	case dataType::RTted:
	case dataType::Posters:
	{
		if (mCurrentTreatedPanelUserIndex < userlist.size())
		{
			u64 user = userlist[mCurrentTreatedPanelUserIndex].first;
			auto& tstinit = mPanelRetreivedUsers.getUserStruct(user);
			bool alreadyInit = (tstinit.mFollowersCount + tstinit.mFollowingCount + tstinit.mName.length()) > 0;

			if ((!alreadyInit) && (!TwitterConnect::LoadUserStruct(user, tstinit, false)))
			{
				SP<CoreFSM> fsm = mFsm;

				auto userDetailState = getFSMState(fsm, TwitterAnalyser, GetUserDetail);

				userDetailState->nextTransition = "Pop";
				userDetailState->mUserID = user;


				GetUpgrador()->activateTransition("getuserdetailtransition");
			}
			else
			{
				// just add user to panel user stats
				auto& currentUserData = mPerPanelUsersStats[user];

				// refresh all 
				for (size_t i = 0; i <= mCurrentTreatedPanelUserIndex; i++)
				{
					u64 refreshuser = userlist[i].first;
					mInStatsUsers[refreshuser] = std::pair<unsigned int, TwitterConnect::UserStruct>(userlist[i].second, mPanelRetreivedUsers.getUserStruct(refreshuser));
				}

				mCurrentTreatedPanelUserIndex++;
				mTreatedUserCount++;
				if (mPanelRetreivedUsers.getUserStruct(user).mFollowersCount || mPanelRetreivedUsers.getUserStruct(user).mFollowingCount)
					mValidUserCount++;

				GetUpgrador()->popState();
			}
		}
		else
		{
			// refresh all 
			for (auto i : userlist)
			{
				u64 refreshuser = i.first;
				mInStatsUsers[refreshuser] = std::pair<unsigned int, TwitterConnect::UserStruct>(i.second, mPanelRetreivedUsers.getUserStruct(refreshuser));
			}
			mUserPanelSize = mValidUserCount;
		}
		break;
	}

	case dataType::Followers:
	case dataType::Following:
	{
		mUserPanelSize = userlist.size();
		if (mCurrentTreatedPanelUserIndex < userlist.size())
		{
			u64 user = userlist[mCurrentTreatedPanelUserIndex].first;

			auto& tstinit = mPanelRetreivedUsers.getUserStruct(user);
			bool alreadyInit = (tstinit.mFollowersCount + tstinit.mFollowingCount + tstinit.mName.length()) > 0;

			if ((!alreadyInit) && (!TwitterConnect::LoadUserStruct(user, tstinit, false)))
			{
				SP<CoreFSM> fsm = mFsm;

				auto userDetailState = getFSMState(fsm, TwitterAnalyser, GetUserDetail);

				userDetailState->nextTransition = "Pop";
				userDetailState->mUserID = user;


				GetUpgrador()->activateTransition("getuserdetailtransition");
			}
			else
			{
				// just add user to panel user stats
				auto& currentUserData = mPerPanelUsersStats[user];

				mInStatsUsers[user] = std::pair<unsigned int, TwitterConnect::UserStruct>(mPanelRetreivedUsers.getUserStruct(user).mFollowersCount, mPanelRetreivedUsers.getUserStruct(user));
				mCurrentTreatedPanelUserIndex++;
				mTreatedUserCount++;
				if(mPanelRetreivedUsers.getUserStruct(user).mFollowersCount || mPanelRetreivedUsers.getUserStruct(user).mFollowingCount)
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