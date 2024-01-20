#pragma once

#include "DataDrivenBaseApplication.h"
#include "HTTPConnect.h"
#include "Texture.h"
#include "YoutubeConnect.h"
#include "GraphDrawer.h"

namespace Kigs
{
	namespace Http
	{
		class ResourceDownloader;
	}
	using namespace Kigs::DDriven;
	using namespace Kigs::Http;

	class YoutubeAnalyser : public DataDrivenBaseApplication
	{
	public:
		DECLARE_CLASS_INFO(YoutubeAnalyser, DataDrivenBaseApplication, Core);
		DECLARE_CONSTRUCTOR(YoutubeAnalyser);

		// give direct access to members
		friend class GraphDrawer;

		// current application state 
		unsigned int					mState = 0;
		// general application parameters
		// wanted sample size
		unsigned int					mSubscribedUserCount = 100;
		unsigned int					mMaxChannelCount = 16;
		int								mMaxUserPerVideo = 0;
		float							mValidChannelPercent = 0.02f;
		// if set, don't get videolist from channel but from keyword
		std::string						mUseKeyword = "";

		// manage a list of users 
		class UserList
		{
		public:

			// return true if new one
			bool	addUser(std::string id)
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
					mUserList[(*f).second].second++;
				}
				return newone;
			}


			bool	addUser(const std::pair<std::string, u32>& user)
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
					mUserList[(*f).second].second += user.second;
				}
				return newone;
			}

			void	addUsers(const std::vector<std::string>& users)
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

			const std::vector<std::pair<std::string, u32>>& getList() const
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

			bool hasUser(const std::string& u)
			{
				return (mUserIndexInVector.find(u) != mUserIndexInVector.end());
			}

		protected:
			// pair of userID / user occurence count (for favorites, likers...)
			std::vector<std::pair<std::string, u32>>			mUserList;
			// given userID, find index in previous vector
			std::unordered_map<std::string, size_t>				mUserIndexInVector;
		};


		void	askUserDetail(const std::string& userID)
		{
			if (mAlreadyAskedUserDetail.find(userID) == mAlreadyAskedUserDetail.end())
			{
				mAlreadyAskedUserDetail.insert(userID);
				mUserDetailsAsked.push_back(userID);
				mNeedUserListDetail = true;
			}
		}

		bool	isUserOf(const std::string& paneluser,const std::string& account) const
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


	protected:

		void	createFSMStates();


		// user detail asked
		std::vector<std::string>			mUserDetailsAsked;
		std::set<std::string>				mAlreadyAskedUserDetail;
		maBool	mNeedUserListDetail = BASE_ATTRIBUTE(NeedUserListDetail, false);



		void	DrawForceBased();
		bool	mDrawForceBased = false;
		v2f		mThumbcenter;
		float	mForcedBaseStartingTime = 0.0f;

		void	ProtectedInit() override;
		void	ProtectedUpdate() override;
		void	ProtectedClose() override;

		void	refreshAllThumbs();

		void	prepareForceGraphData();

		void	switchDisplay();
		void	switchForce();
		void	switchUnsub();
		void	mainChannelID(const std::string& id);
		void	getChannelStats(const YoutubeConnect::ChannelStruct& chan);
		void	requestDone();
		bool	checkDone();
		void	QuotaExceeded(int errorCode);

		WRAP_METHODS( switchDisplay, switchForce, switchUnsub, mainChannelID, getChannelStats, checkDone, QuotaExceeded);

		// utility
		CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false);

		void		SaveStatFile();

		void	ProtectedInitSequence(const std::string& sequence) override;
		void	ProtectedCloseSequence(const std::string& sequence) override;

		// global data
		SP<HTTPConnect>									mGoogleConnect = nullptr;
		std::string										mChannelName;
		std::string										mChannelID;

		YoutubeConnect::ChannelStruct					mChannelInfos;

		unsigned int									mySubscribedWriters = 0;
		unsigned int									myPublicWriters = 0;


		//all treated users
		std::unordered_map<std::string, YoutubeConnect::UserStruct>		mFoundUsers;

		// all subscribed authors infos
		std::unordered_map<std::string, YoutubeConnect::ChannelStruct>	mSubscribedAuthorInfos;

		// all not subscribed authors infos
		std::unordered_map<std::string, YoutubeConnect::ChannelStruct>	mNotSubscribedAuthorInfos;

		// for each user in panel, list of (likers, followers, retwetters...)
		std::map<std::string, UserList>					mPerPanelUsersStats;

		std::unordered_map<std::string, YoutubeConnect::ChannelStruct*>	mFoundChannels;

		// display data
		SP<YoutubeConnect>								mYoutubeConnect = nullptr;
		CMSP											mMainInterface;
		SP<GraphDrawer>									mGraphDrawer = nullptr;
		double											mLastUpdate;

		// current process data
		std::vector<YoutubeConnect::videoStruct>			mVideoListToProcess;
		std::set<std::string>								mAlreadyTreatedAuthors;
		unsigned int										mCurrentProcessedVideo = 0;
		std::string											mCurrentProcessedUser;
		unsigned int										mCurrentVideoUserFound = 0;
		std::vector<std::pair<std::string, std::pair<usString, std::string>> >	mTmpUserChannels;

		bool mBadVideo = false;
		int  mNotSubscribedUserForThisVideo = 0;

		// get current time once at launch to compare with modified file date
		time_t     mCurrentTime = 0;

		// if a file is older ( in seconds ) than this limit, then it's considered as not existing ( to recreate )
		double		mOldFileLimit = 0.0;

		std::string	mFromDate = "";
		std::string	mToDate = "";


		enum Measure
		{
			Percent = 0,
			Similarity = 1,
			Normalized = 2,
			MEASURE_COUNT = 3
		};

		const std::string	mUnity[MEASURE_COUNT] = { "\%","sc","n" };

		Measure		mCurrentMeasure = Percent;

		CMSP												mFsm;
		std::unordered_map<KigsID, SP<CoreFSMTransition>>	mTransitionList;

		// wait request was treated
		maBool	mNeedWait = BASE_ATTRIBUTE(NeedWait, false);

		maInt	mErrorCode = BASE_ATTRIBUTE(ErrorCode, 0);
	};
}
