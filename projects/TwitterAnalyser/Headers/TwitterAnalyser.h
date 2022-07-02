#pragma once

#include <DataDrivenBaseApplication.h>
#include "CoreFSMState.h"
#include "TwitterConnect.h"
#include "ScrapperManager.h"
#include "GraphDrawer.h"


class TwitterAnalyser : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(TwitterAnalyser, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(TwitterAnalyser);

	// give direct access to members
	friend class GraphDrawer;
	enum class dataType
	{
		// panel types
		Likers		= 0,			// with or without hashtag, with or without period
		Posters		= 1,			// with hashtag, with or without period
		// panel or analysed types
		Followers	= 2,			// no hashtag, no period here
		Following	= 3,			// no hashtag, no period here

		// analysed types only
		Favorites	= 4,			// with or without hashtag, with or without period
		TOP			= 5,			// if used for data type, then just work with panel

		// panel or analysed types
		RTters		= 6,
		RTted		= 7,
	};
protected:

	std::vector<std::string>	PanelUserName = { "Likers" , "Posters" , "Followers" , "Following", "Favorites" , "Top" , "Retweeters" , "Retweeted"};

	void	requestDone();
	void	mainUserDone(TwitterConnect::UserStruct& CurrentUserStruct);
	void	switchForce();
	void	switchDisplay();

	void	initLogos();

	void	manageRetrievedUserDetail(TwitterConnect::UserStruct& CurrentUserStruct);
	bool	checkDone();

	WRAP_METHODS(requestDone, mainUserDone, switchDisplay, switchForce, manageRetrievedUserDetail, checkDone);

	void		commonStatesFSM();
	std::unordered_map<KigsID, SP<CoreFSMTransition>>	mTransitionList;

	std::string	searchFavoritesFSM();
	std::string	searchLikersFSM();
	std::string	searchPostersFSM();
	std::string	searchFollowFSM(const std::string& followtype);
	std::string	searchRetweetersFSM();
	std::string	searchRetweetedFSM();

	void		TopFSM(const std::string& laststate);

	void	analyseFavoritesFSM(const std::string& lastState);
	void	analyseLikersFSM(const std::string& lastState);
	void	analyseFollowFSM(const std::string& lastState, const std::string& followtype);
	void	analyseRetweetersFSM(const std::string& lastState);
	void	analyseRetweetedFSM(const std::string& lastState);


	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;

	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;

	SP<TwitterConnect>			mTwitterConnect=nullptr;
	CMSP						mMainInterface=nullptr;

	SP<GraphDrawer>				mGraphDrawer = nullptr;

	
	u32									mValidUserCount=0;
	u32									mTreatedUserCount = 0;
	u32									mMaxUsersPerTweet = 0;
	u32									mMaxUserCount=45;
	u32									mUserPanelSize=500;

	bool								mCanGetMoreUsers = false;

	// analyse type
	dataType		mPanelType = dataType::Followers;
	dataType		mAnalysedType = dataType::Following;

	bool			mUseHashTags = false;
	float			mValidUserPercent;
	// when retrieving followers
	u32				mWantedTotalPanelSize = 100000;

	// wait request was treated
	maBool	mNeedWait = BASE_ATTRIBUTE(NeedWait, false);


	// manage a list of users 
	class UserList
	{
	public:

		// return true if new one
		bool	addUser(u64 id)
		{
			bool newone = false;
			auto f = mUserIndexInVector.find(id);
			if (f == mUserIndexInVector.end())
			{
				mUserIndexInVector[id] = mUserList.size();
				mUserList.push_back({ id,1 });
				newone = true;
			}
			else
			{
				mUserList[(*f).second].second ++;
			}
			return newone;
		}


		bool	addUser(const std::pair<u64, u32>& user)
		{
			bool newone = false;
			auto f = mUserIndexInVector.find(user.first);
			if (f == mUserIndexInVector.end())
			{
				mUserIndexInVector[user.first] = mUserList.size();
				mUserList.push_back(user);
				newone = true;
			}
			else
			{
				mUserList[(*f).second].second+= user.second;
			}
			return newone;
		}

		void	addUsers(const std::vector<u64>& users)
		{
			for (auto& u : users)
			{
				addUser(u);
			}
		}

		void	addUsers(const UserList& users)
		{
			for (auto& u : users.getList())
			{
				addUser(u);
			}
		}

		const std::vector<std::pair<u64, u32>>& getList() const
		{
			return mUserList;
		}

		size_t	size() const
		{
			return mUserList.size();
		}

		void	clear()
		{
			mUserIndexInVector.clear();
			mUserList.clear();
		}

	protected:
		// pair of userID / user occurence count (for favorites, likers...)
		std::vector<std::pair<u64, u32>>			mUserList;
		// given userID, find index in previous vector
		std::unordered_map<u64, size_t>				mUserIndexInVector;
	};

	UserList		mPanelUserList;
	// current panel user on which statistics are made
	u32				mCurrentTreatedPanelUserIndex = 0;

	class UserListWithStruct : public UserList
	{
	public:
		void	addUser(u64 id)
		{
			UserList::addUser(id);
			resizeUserStruct();
		}

		void	addUser(const std::pair<u64, u32>& user)
		{
			UserList::addUser(user);
			resizeUserStruct();
		}

		void	addUsers(const std::vector<u64>& users)
		{
			UserList::addUsers(users);
			resizeUserStruct();
		}

		void	addUsers(const UserList& users)
		{
			UserList::addUsers(users);
			resizeUserStruct();
		}

		TwitterConnect::UserStruct& getUserStruct(u64 id)
		{
			auto f = mUserIndexInVector.find(id);
			if (f != mUserIndexInVector.end())
			{
				return mUsersStruct[(*f).second];
			}
			addUser(id);
			return getUserStruct(id);
		}

		TwitterConnect::UserStruct& getUserStructAtIndex(size_t index)
		{
			return mUsersStruct[index];
		}

	protected:
		void	resizeUserStruct()
		{
			size_t prevsize = mUsersStruct.size();
			if (prevsize < mUserList.size())
			{
				mUsersStruct.resize(mUserList.size());

				for (size_t i = prevsize; i < mUsersStruct.size(); i++)
				{
					mUsersStruct[i].mID = mUserList[i].first;
				}
			}
		}
		std::vector<TwitterConnect::UserStruct>			mUsersStruct;
	};

	void		SaveStatFile();

	// user 0 is main user if needed
	UserListWithStruct	mPanelRetreivedUsers;
	
	// for each user in panel, list of (likers, followers, retwetters...)
	std::map<u64, UserList>					mPerPanelUsersStats;

	// count number of time a user appears in stats
	std::map<u64, std::pair<u32, TwitterConnect::UserStruct>>			mInStatsUsers;


	void	askUserDetail(u64 userID)
	{
		if (mAlreadyAskedUserDetail.find(userID) == mAlreadyAskedUserDetail.end())
		{
			mAlreadyAskedUserDetail.insert(userID);
			mUserDetailsAsked.push_back(userID);
			mNeedUserListDetail = true;
		}
	}

	const TwitterConnect::UserStruct& getRetreivedUser(u64 uid)
	{
		return mPanelRetreivedUsers.getUserStruct(uid);
	}

	// user detail asked
	std::vector<u64>			mUserDetailsAsked;
	std::set<u64>				mAlreadyAskedUserDetail;
	maBool	mNeedUserListDetail = BASE_ATTRIBUTE(NeedUserListDetail, false);

	CMSP mFsm;

	bool	isUserOf(u64 paneluser, u64 account) const
	{
		const auto& found = mPerPanelUsersStats.find(paneluser);
		if (found != mPerPanelUsersStats.end())
		{
			for (auto& c : (*found).second.getList())
			{
				if (c.first == account)
				{
					return true;
				}
			}
		}
		return false;
	}
};

