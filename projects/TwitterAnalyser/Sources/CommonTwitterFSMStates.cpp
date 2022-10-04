#include "CommonTwitterFSMStates.h"


void CoreFSMStartMethod(TwitterAnalyser, InitUser)
{

}

void CoreFSMStopMethod(TwitterAnalyser, InitUser)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, InitUser))
{
	std::string currentUserProgress = "Cache/";
	currentUserProgress += "UserName/";
	currentUserProgress += mPanelRetreivedUsers.getUserStructAtIndex(0).mName.ToString() + ".json";
	CoreItemSP currentP = TwitterConnect::LoadJSon(currentUserProgress);

	if (!currentP) // new user
	{
		KigsCore::Connect(mTwitterConnect.get(), "UserDetailRetrieved", this, "mainUserDone");
		mTwitterConnect->launchUserDetailRequest(mPanelRetreivedUsers.getUserStructAtIndex(0).mName.ToString(), mPanelRetreivedUsers.getUserStructAtIndex(0));
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else // load current user
	{
		u64 userID = currentP["id"];
		mTwitterConnect->LoadUserStruct(userID, mPanelRetreivedUsers.getUserStructAtIndex(0), true);
		auto availableTransitions = GetUpgrador()->getTransitionList();
		for (const auto& t : availableTransitions)
		{
			// Init only have 2 transitions : "waittransition" and another
			// so active the other one
			if (t != "waittransition")
			{
				GetUpgrador()->activateTransition(t);
				break;
			}
		}
	}
	return false;
}


void	CoreFSMStartMethod(TwitterAnalyser, GetUserDetail)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetUserDetail)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetUserDetail))
{
	// back from next transition ?
	if (GetUpgrador()->nextTransition == "")
	{
		GetUpgrador()->popState();
		return false;
	}

	u64 userID = GetUpgrador()->mUserID;

	if (!mTwitterConnect->LoadUserStruct(userID, mPanelRetreivedUsers.getUserStruct(userID), false))
	{
		KigsCore::Connect(mTwitterConnect.get(), "UserDetailRetrieved", this, "manageRetrievedUserDetail");
		mTwitterConnect->launchUserDetailRequest(userID, mPanelRetreivedUsers.getUserStruct(userID));
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else
	{
		GetUpgrador()->activateTransition(GetUpgrador()->nextTransition);
		// go to next only once
		GetUpgrador()->nextTransition = "";
	}
	return false;
}

void	CoreFSMStartMethod(TwitterAnalyser, Wait)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, Wait)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, Wait))
{

	return false;
}

void	CoreFSMStartMethod(TwitterAnalyser, BaseStepState)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, BaseStepState)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, BaseStepState))
{

	return false;
}


void	CoreFSMStartMethod(TwitterAnalyser, GetUserListDetail)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetUserListDetail)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetUserListDetail))
{
	if (!mUserDetailsAsked.size())
	{
		mNeedUserListDetail = false;
		GetUpgrador()->popState();
		return false;
	}

	u64 userID = mUserDetailsAsked.back();

	if (!TwitterConnect::LoadUserStruct(userID, GetUpgrador()->mTmpUserStruct, false))
	{
		KigsCore::Connect(mTwitterConnect.get(), "UserDetailRetrieved", this, "manageRetrievedUserDetail");
		mTwitterConnect->launchUserDetailRequest(userID, GetUpgrador()->mTmpUserStruct);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	mUserDetailsAsked.pop_back();

	return false;
}


void	CoreFSMStartMethod(TwitterAnalyser, Done)
{
	mGraphDrawer->setEverythingDone();

	SaveStatFile();
}
void	CoreFSMStopMethod(TwitterAnalyser, Done)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, Done))
{

	return false;
}




void CoreFSMStartMethod(TwitterAnalyser, InitHashTag)
{

}

void CoreFSMStopMethod(TwitterAnalyser, InitHashTag)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, InitHashTag))
{
	// nothing more to do, just go to next state
	auto availableTransitions = GetUpgrador()->getTransitionList();
	for (const auto& t : availableTransitions)
	{
		// Init only have 2 transitions : "waittransition" and another
		// so active the other one
		if (t != "waittransition")
		{
			GetUpgrador()->activateTransition(t);
			break;
		}
	}
	return false;
}


void	CoreFSMStartMethod(TwitterAnalyser, GetTweets)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetTweets)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetTweets))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	bool needMoreTweet=true;
	bool hasTweetFile=false;

	if (TwitterConnect::LoadTweetsFile(GetUpgrador()->mTweets, GetUpgrador()->mUserName))
	{
		hasTweetFile = true;
		// if enough tweets or can't get more
		if ( (GetUpgrador()->mNeededTweetCount && (GetUpgrador()->mNeededTweetCount <= GetUpgrador()->mTweets.size())) || GetUpgrador()->mCantGetMoreTweets)
		{
			needMoreTweet = false;
		}

		// limit tweets in time
		if (GetUpgrador()->mTimeLimit)
		{
			auto currentTime = time(0);

			u64 timelimitinseconds = GetUpgrador()->mTimeLimit * 24 * 60 * 60;

			size_t offlimit = 0;
			for (const auto& twt : GetUpgrador()->mTweets)
			{
				time_t cr = twt.getCreationDate();

				double diffseconds = difftime(currentTime, cr);
				if (diffseconds > timelimitinseconds)
				{
					offlimit++;
					if (offlimit >= 3)
					{
						needMoreTweet = false;
						break;
					}	
				}
				
			}
		}
	}

	if (needMoreTweet)
	{
		std::string nextCursor = "-1";
		if (hasTweetFile)
		{
			std::string filenamenext_token = "Cache/UserName/";
			if (TwitterConnect::useDates())
			{
				filenamenext_token += "_" + TwitterConnect::getDate(0) + "_" + TwitterConnect::getDate(1) + "_";
			}
			filenamenext_token += GetUpgrador()->mUserName + "_TweetsNextCursor.json";
			auto nxtTokenJson = TwitterConnect::LoadJSon(filenamenext_token);

			if (nxtTokenJson)
			{
				nextCursor = nxtTokenJson["next-cursor"]->toString();
			}
			if (nextCursor == "-1")
			{
				GetUpgrador()->mCantGetMoreTweets = true;
			}
		}

		// more tweets
		if ( (nextCursor != "-1") || (!hasTweetFile))
		{
			KigsCore::Connect(mTwitterConnect.get(), "TweetRetrieved", this, "manageRetrievedTweets");
			if (GetUpgrador()->mSearchTweets)
			{
				mTwitterConnect->launchSearchTweetRequest(GetUpgrador()->mHashTag, nextCursor);
			}
			else
			{
				mTwitterConnect->launchGetTweetRequest(GetUpgrador()->mUserID, GetUpgrador()->mUserName, nextCursor);
			}
			mNeedWait = true;
			GetUpgrador()->activateTransition("waittransition");
			return false;
		}
	}
	GetUpgrador()->popState();
	return false;
}

void	CoreFSMStateClassMethods(TwitterAnalyser, GetTweets)::manageRetrievedTweets(std::vector<TwitterConnect::Twts>& twtlist, const std::string& nexttoken)
{
	std::string filenamenext_token = "Cache/UserName/";
	if (TwitterConnect::useDates())
	{
		filenamenext_token += "_" + TwitterConnect::getDate(0) + "_" + TwitterConnect::getDate(1) + "_";
	}
	filenamenext_token += GetUpgrador()->mUserName + "_TweetsNextCursor.json";

	std::vector<TwitterConnect::Twts>	v;
	TwitterConnect::LoadTweetsFile(v, GetUpgrador()->mUserName);
	v.insert(v.end(), twtlist.begin(), twtlist.end());

	if (nexttoken != "-1")
	{
		CoreItemSP currentUserJson = MakeCoreMap();
		currentUserJson->set("next-cursor", nexttoken);
		TwitterConnect::SaveJSon(filenamenext_token, currentUserJson);
	}
	else
	{
		GetUpgrador()->mCantGetMoreTweets = true;
		ModuleFileManager::RemoveFile(filenamenext_token.c_str());
		//don't randomize tweet vector
	}

	KigsCore::Disconnect(mTwitterConnect.get(), "TweetRetrieved", this, "manageRetrievedTweets");
	TwitterConnect::SaveTweetsFile(v, GetUpgrador()->mUserName);

	requestDone();
}

void	CoreFSMStartMethod(TwitterAnalyser, GetUsers)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetUsers)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetUsers))
{

	if (GetUpgrador()->mNeedMoreUsers) // this state needs more user
	{
		if (GetUpgrador()->mCantGetMoreUsers) // no more users can be retreived
		{
			GetUpgrador()->mNeedMoreUsers = false;
		}
		else
		{
			if (GetUpgrador()->mNeededUserCountIncrement)
			{
				GetUpgrador()->mNeededUserCount += GetUpgrador()->mNeededUserCountIncrement;
			}
		}
	}

	return false;
}


void	CoreFSMStartMethod(TwitterAnalyser, RetrieveTweetActors)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, RetrieveTweetActors)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, RetrieveTweetActors))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	SP<CoreFSM> fsm = mFsm;

	switch (GetUpgrador()->mStateStep)
	{
	case 0:
		// get tweets and go to state step 1
	{
		GetUpgrador()->activateTransition("retrievetweetstransition");
		GetUpgrador()->mStateStep = 1;
		GetUpgrador()->mCanGetMoreActors = false;
	}
	break;
	case 1:
		// for each retrieved tweet, get actors
	{
		auto tweetsState = fsm->getState("RetrieveTweets")->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();
		if (GetUpgrador()->mCurrentTreatedTweetIndex < tweetsState->mTweets.size())
		{
			auto& currentTweet = tweetsState->mTweets[GetUpgrador()->mCurrentTreatedTweetIndex];
			GetUpgrador()->mTreatedActorPerTweet.push_back({ 0,0 });

			if (((currentTweet.mLikeCount) && (GetUpgrador()->mActorType == "Likers"))
				|| ((currentTweet.mRetweetCount) && (GetUpgrador()->mActorType == "Retweeters")))

			{
				auto getActorState = fsm->getState("Get" + GetUpgrador()->mActorType)->as<CoreFSMStateClass(TwitterAnalyser, GetUsers)>();
				getActorState->mForID = currentTweet.mTweetID;
				GetUpgrador()->mStateStep = 2;
				GetUpgrador()->activateTransition("getactorstransition");
			}
			else if ((GetUpgrador()->mActorType == "Posters") || (GetUpgrador()->mActorType == "Retweeted") || (GetUpgrador()->mActorType == "Interactors"))
			{
				auto getActorState = fsm->getState("Get" + GetUpgrador()->mActorType)->as<CoreFSMStateClass(TwitterAnalyser, GetUsers)>();
				// store authorID here
				getActorState->mForID = currentTweet.mAuthorID;
				if ( (GetUpgrador()->mActorType == "Interactors") && (currentTweet.mReplyedTo)) // for interactors get rtted or replied to
				{
					getActorState->mForID = currentTweet.mReplyedTo;
				}
				GetUpgrador()->mStateStep = 2;
				GetUpgrador()->activateTransition("getactorstransition");
			}
			else if ( (GetUpgrador()->mActorType == "Replyers") && (currentTweet.mConversationID!=-1))
			{
				auto getActorState = fsm->getState("Get" + GetUpgrador()->mActorType)->as<CoreFSMStateClass(TwitterAnalyser, GetUsers)>();
				// store authorID here
				getActorState->mForID = currentTweet.mConversationID;
				GetUpgrador()->mStateStep = 2;
				GetUpgrador()->activateTransition("getactorstransition");
			}
			else
			{
				GetUpgrador()->mCurrentTreatedTweetIndex++;
			}
		}
		else
		{
			// ask for more
			GetUpgrador()->mStateStep = 3;
		}
		
	}
	break;
	case 2:
		// some actors were retrieved for current tweet
	{
		auto getActorState = fsm->getState("Get" + GetUpgrador()->mActorType)->as<CoreFSMStateClass(TwitterAnalyser, GetUsers)>();
		std::pair<u32,u32>& currentTreatedActor = GetUpgrador()->mTreatedActorPerTweet[GetUpgrador()->mCurrentTreatedTweetIndex];

		if(currentTreatedActor.first==0)// do it only once per tweet
		{
			auto tweetsState = fsm->getState("RetrieveTweets")->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();
			auto& currentTweet = tweetsState->mTweets[GetUpgrador()->mCurrentTreatedTweetIndex];
			
			auto userlistsize = getActorState->mUserlist.size();
			
			// different for each actor type
			if(GetUpgrador()->mActorType == "Likers")
			{
				if (userlistsize > currentTweet.mLikeCount)
				{
					currentTweet.mLikeCount = userlistsize;
				}
				tweetsState->mTweetRetrievedUserCount[currentTweet.mTweetID] = { currentTweet.mLikeCount, userlistsize };
			}
			else if (GetUpgrador()->mActorType == "Retweeters")
			{
				if (userlistsize > currentTweet.mRetweetCount)
				{
					currentTweet.mRetweetCount = userlistsize;
				}
				tweetsState->mTweetRetrievedUserCount[currentTweet.mTweetID] = { currentTweet.mRetweetCount, userlistsize };
			}
			else if ( (GetUpgrador()->mActorType == "Posters") || (GetUpgrador()->mActorType == "Retweeted") || (GetUpgrador()->mActorType == "Replyers") || (GetUpgrador()->mActorType == "Interactors"))
			{
				tweetsState->mTweetRetrievedUserCount[currentTweet.mTweetID] = { 1, userlistsize };
			}
		}

		if ((GetUpgrador()->mWantedActorCount) && (GetUpgrador()->mUserlist.size() >= GetUpgrador()->mWantedActorCount)) // enough actors where found
		{
			GetUpgrador()->mStateStep = 4;
			break;
		}

		bool doelse = false;
		if (GetUpgrador()->mTreatFullList)
		{
			if (currentTreatedActor.first < getActorState->mUserlist.size())
			{
				for (auto user : getActorState->mUserlist.getList())
				{
					if (!(GetUpgrador()->mExcludeMainUser && (getActorState->mUserlist.getList()[currentTreatedActor.first].first == mPanelRetreivedUsers.getUserStructAtIndex(0).mID)))
					{
						if (GetUpgrador()->mUserlist.addUser(user))
						{
							currentTreatedActor.second++;
						}
					}
					currentTreatedActor.first++;
				}
				
				GetUpgrador()->activateTransition("getuserdatatransition");
				
			}
			else
			{
				doelse = true;
			}
			
		}
		else
		{
			if ((currentTreatedActor.first < getActorState->mUserlist.size()) && ((currentTreatedActor.second < GetUpgrador()->mMaxActorPerTweet) || (GetUpgrador()->mMaxActorPerTweet == 0)))
			{
				if (!(GetUpgrador()->mExcludeMainUser && (getActorState->mUserlist.getList()[currentTreatedActor.first].first == mPanelRetreivedUsers.getUserStructAtIndex(0).mID)))
				{
					if (GetUpgrador()->mUserlist.addUser(getActorState->mUserlist.getList()[currentTreatedActor.first]))
					{
						currentTreatedActor.second++;
						if (!GetUpgrador()->mTreatAllActorsTogether)
						{
							GetUpgrador()->activateTransition("getuserdatatransition");
						}
					}
				}
				currentTreatedActor.first++;
			}
			else
			{
				doelse = true;
			}
		}

		if(doelse)
		{
			if (currentTreatedActor.first < getActorState->mUserlist.size())
			{
				// it's possible to get more actors for this tweet
				GetUpgrador()->mCanGetMoreActors = true;
			}
			// treat next tweet 
			GetUpgrador()->mCurrentTreatedTweetIndex++;
			GetUpgrador()->mStateStep = 1;
		}
	}
	break;

	case 3:
		// we need more tweets, more actors per tweet or consider we can't get any more (so everything is done)
	{
		auto tweetsState = fsm->getState("RetrieveTweets")->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();
		// need more tweets ?
		if (tweetsState->mCanGetMore)
		{
			tweetsState->mAskMore = true;
			GetUpgrador()->mStateStep = 0;
		}
		// need more users per tweet ?
		else
		{
			if ( (GetUpgrador()->mMaxActorPerTweet) && (GetUpgrador()->mCanGetMoreActors))
			{
				// ask for more actors
				u32 added = GetUpgrador()->mMaxActorPerTweet / 2;
				GetUpgrador()->mMaxActorPerTweet += (added>1)?added:2;

				// restart from first tweet (but ask more actors)
				GetUpgrador()->mCurrentTreatedTweetIndex=0;
				GetUpgrador()->mStateStep = 1;
				GetUpgrador()->mCanGetMoreActors = false;
			}
			else
			{
				// I just can't get enough !
				GetUpgrador()->mStateStep = 0;

				if (GetUpgrador()->getTransition("donetransition") && !GetUpgrador()->mTreatAllActorsTogether)
				{
					if (mAnalysedType == dataType::TOP) // for top, call it one last time
					{
						GetUpgrador()->activateTransition("getuserdatatransition");
					}
					break;
					mUserPanelSize = mValidUserCount;
					
				}
				else
				{
					if(GetUpgrador()->mTreatAllActorsTogether)
					{
						GetUpgrador()->mStateStep = 4;
						break;
					}

					GetUpgrador()->mStateStep = 5;
				}
			}
		}
	}
	break;
	case 4:
	{
		GetUpgrador()->activateTransition("getuserdatatransition");
		GetUpgrador()->mStateStep = 5;
	}
	break;
	case 5:
	{
		auto rtweetsState = fsm->getState("RetrieveTweets")->as<CoreFSMStateClass(TwitterAnalyser, RetrieveTweets)>();
		
		// reset RetrieveTweets state
		rtweetsState->reset();

		auto tweetsState = fsm->getState("GetTweets")->as<CoreFSMStateClass(TwitterAnalyser, GetTweets)>();
		tweetsState->reset();
		GetUpgrador()->popState();
		GetUpgrador()->mStateStep = 0;
		GetUpgrador()->mTreatedActorPerTweet.clear();
		GetUpgrador()->mCurrentTreatedTweetIndex=0;
	}
	}

	return false;
}

void	CoreFSMStateClassMethods(TwitterAnalyser, RetrieveTweetActors)::copyUserList(TwitterAnalyser::UserList& touserlist)
{
	touserlist = GetUpgrador()->mUserlist;
}


void	CoreFSMStartMethod(TwitterAnalyser, GetLikers)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetLikers)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetLikers))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	// call parent upgrador update
	UpgradorMethodParentClass::UpgradorUpdate(timer, addParam);

	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(GetUpgrador()->mForID) + "_LikersNextCursor.json";

	CoreItemSP nextt = TwitterConnect::LoadJSon(filenamenext_token);

	std::string next_cursor = "-1";

	std::vector<u64>	 v;
	bool found= TwitterConnect::LoadLikersFile(GetUpgrador()->mForID,v);
	if (nextt)
	{
		if (!(GetUpgrador()->mNeededUserCount) || (v.size() < GetUpgrador()->mNeededUserCount))
		{
			next_cursor = nextt["next-cursor"]->toString();
		}
	}

	if ((!found) || (next_cursor != "-1"))
	{
		KigsCore::Connect(mTwitterConnect.get(), "LikersRetrieved", this, "manageRetrievedLikers");
		mTwitterConnect->launchGetLikers(GetUpgrador()->mForID, next_cursor);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else
	{
		GetUpgrador()->mUserlist.clear();
		GetUpgrador()->mUserlist.addUsers(v);
		GetUpgrador()->popState();
	}
		
	return false;
}


void	CoreFSMStateClassMethods(TwitterAnalyser, GetLikers)::manageRetrievedLikers(std::vector<u64>& TweetLikers, const std::string& nexttoken)
{
	u64 tweetID = GetUpgrador()->mForID;

	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(tweetID) + "_LikersNextCursor.json";

	std::vector<u64> v;
	TwitterConnect::LoadLikersFile(tweetID,v);
	v.insert(v.end(), TweetLikers.begin(), TweetLikers.end());

	if (nexttoken != "-1")
	{
		CoreItemSP currentUserJson = MakeCoreMap();
		currentUserJson->set("next-cursor", nexttoken);
		TwitterConnect::SaveJSon(filenamenext_token, currentUserJson);
	}
	else
	{
		ModuleFileManager::RemoveFile(filenamenext_token.c_str());
		TwitterConnect::randomizeVector(v);
	}

	KigsCore::Disconnect(mTwitterConnect.get(), "LikersRetrieved", this, "manageRetrievedLikers");
	TwitterConnect::SaveLikersFile(v, tweetID);

	requestDone();
}

void	CoreFSMStartMethod(TwitterAnalyser, GetReplyers)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetReplyers)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetReplyers))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	// call parent upgrador update
	UpgradorMethodParentClass::UpgradorUpdate(timer, addParam);

	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(GetUpgrador()->mForID) + "_ReplyersNextCursor.json";

	CoreItemSP nextt = TwitterConnect::LoadJSon(filenamenext_token);

	std::string next_cursor = "-1";

	std::vector<u64>	 v;
	bool found = TwitterConnect::LoadReplyersFile(GetUpgrador()->mForID, v);
	if (nextt)
	{
		if (!(GetUpgrador()->mNeededUserCount) || (v.size() < GetUpgrador()->mNeededUserCount))
		{
			next_cursor = nextt["next-cursor"]->toString();
		}
	}

	if ((!found) || (next_cursor != "-1"))
	{
		KigsCore::Connect(mTwitterConnect.get(), "ReplyersRetrieved", this, "manageRetrievedReplyers");
		mTwitterConnect->launchGetReplyers(GetUpgrador()->mForID, next_cursor);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else
	{
		GetUpgrador()->mUserlist.clear();
		GetUpgrador()->mUserlist.addUsers(v);
		GetUpgrador()->popState();
	}

	return false;
}


void	CoreFSMStateClassMethods(TwitterAnalyser, GetReplyers)::manageRetrievedReplyers(std::vector<u64>& TweetReplyers, const std::string& nexttoken)
{
	u64 tweetID = GetUpgrador()->mForID;

	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(tweetID) + "_ReplyersNextCursor.json";

	std::vector<u64> v;
	TwitterConnect::LoadReplyersFile(tweetID, v);
	v.insert(v.end(), TweetReplyers.begin(), TweetReplyers.end());

	if (nexttoken != "-1")
	{
		CoreItemSP currentUserJson = MakeCoreMap();
		currentUserJson->set("next-cursor", nexttoken);
		TwitterConnect::SaveJSon(filenamenext_token, currentUserJson);
	}
	else
	{
		ModuleFileManager::RemoveFile(filenamenext_token.c_str());
		TwitterConnect::randomizeVector(v);
	}

	KigsCore::Disconnect(mTwitterConnect.get(), "ReplyersRetrieved", this, "manageRetrievedReplyers");
	TwitterConnect::SaveReplyersFile(v, tweetID);

	requestDone();
}


void	CoreFSMStartMethod(TwitterAnalyser, GetFavorites)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetFavorites)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetFavorites))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	auto user = GetUpgrador()->mUserID;
		
	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(user) + "_FavsNextCursor.json";

	CoreItemSP nextt = TwitterConnect::LoadJSon(filenamenext_token);

	std::string next_cursor = "-1";
	if (nextt)
	{
		next_cursor = nextt["next-cursor"]->toString();
	}
	bool hasFavoriteList = TwitterConnect::LoadFavoritesFile(user, GetUpgrador()->mFavorites);

	// favorite file exist and enought found or not anymore
	// =>pop
	if (hasFavoriteList && ((GetUpgrador()->mFavorites.size() >= GetUpgrador()->mFavoritesCount) || (next_cursor == "-1")))
	{
		// if favorites were retrieved
		GetUpgrador()->popState();
	}
	else
	{
		KigsCore::Connect(mTwitterConnect.get(), "FavoritesRetrieved", this, "manageRetrievedFavorites");
		mTwitterConnect->launchGetFavoritesRequest(user, next_cursor);
		mNeedWait = true;
		GetUpgrador()->activateTransition("waittransition");
	}

	return false;
}


void	CoreFSMStateClassMethods(TwitterAnalyser, GetFavorites)::manageRetrievedFavorites(std::vector<TwitterConnect::Twts>& favs, const std::string& nexttoken)
{

	auto user = GetUpgrador()->mUserID;

	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(user) + "_FavsNextCursor.json";
	std::vector<TwitterConnect::Twts> v;
	TwitterConnect::LoadFavoritesFile(user, v);
	v.insert(v.end(), favs.begin(), favs.end());

	if (nexttoken != "-1")
	{
		CoreItemSP currentUserJson = MakeCoreMap();
		currentUserJson->set("next-cursor", nexttoken);
		TwitterConnect::SaveJSon(filenamenext_token, currentUserJson);
	}
	else
	{
		ModuleFileManager::RemoveFile(filenamenext_token.c_str());
	}

	TwitterConnect::SaveFavoritesFile(user, v);

	KigsCore::Disconnect(mTwitterConnect.get(), "FavoritesRetrieved", this, "manageRetrievedFavorites");
	requestDone();
}

void	CoreFSMStateClassMethods(TwitterAnalyser, GetFavorites)::copyUserList(TwitterAnalyser::UserList& touserlist)
{
	touserlist.clear();
	for (const auto& u : GetUpgrador()->mFavorites)
	{
		touserlist.addUser(u.mAuthorID);
	}
}

void	CoreFSMStartMethod(TwitterAnalyser, GetFollow)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetFollow)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetFollow))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	// call parent upgrador update
	UpgradorMethodParentClass::UpgradorUpdate(timer, addParam);

	std::string filenamenext_token = "Cache/Users/" + TwitterConnect::GetUserFolderFromID(GetUpgrador()->mForID) + "/";
	filenamenext_token += TwitterConnect::GetIDString(GetUpgrador()->mForID) + "_" + GetUpgrador()->mFollowtype + "_NextCursor.json";

	CoreItemSP nextt = TwitterConnect::LoadJSon(filenamenext_token);

	std::string next_cursor = "-1";

	std::vector<u64>	 v;
	bool				hasFollowFile = false;
	if (nextt)
	{
		next_cursor = nextt["next-cursor"]->toString();
	}
	else
	{
		hasFollowFile = TwitterConnect::LoadFollowFile(GetUpgrador()->mForID, v, GetUpgrador()->mFollowtype);
	}

	if (GetUpgrador()->mNeededUserCount)
	{
		// check limit count
		if (hasFollowFile && (v.size() >= GetUpgrador()->mNeededUserCount)) // if enough, set next_cursor to -1
		{
			next_cursor = "-1";
		}
	}

	if ((!hasFollowFile) || (next_cursor != "-1"))
	{
		KigsCore::Connect(mTwitterConnect.get(), "FollowRetrieved", this, "manageRetrievedFollow");
		mTwitterConnect->launchGetFollow(GetUpgrador()->mForID, GetUpgrador()->mFollowtype, next_cursor);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else
	{
		GetUpgrador()->mUserlist.clear();
		GetUpgrador()->mUserlist.addUsers(v);
		GetUpgrador()->popState();
	}
	return false;
}

void	CoreFSMStateClassMethods(TwitterAnalyser, GetFollow)::manageRetrievedFollow(std::vector<u64>& follow, const std::string& nexttoken)
{
	std::string filenamenext_token = "Cache/Users/" + TwitterConnect::GetUserFolderFromID(GetUpgrador()->mForID) + "/";
	filenamenext_token += TwitterConnect::GetIDString(GetUpgrador()->mForID) + "_" + GetUpgrador()->mFollowtype + "_NextCursor.json";

	std::vector<u64> v;
	bool fexist = TwitterConnect::LoadFollowFile(GetUpgrador()->mForID, v, GetUpgrador()->mFollowtype);
	v.insert(v.end(), follow.begin(), follow.end());

	if (nexttoken != "-1")
	{
		CoreItemSP currentUserJson = MakeCoreMap();
		currentUserJson->set("next-cursor", nexttoken);
		TwitterConnect::SaveJSon(filenamenext_token, currentUserJson);
	}
	else
	{
		GetUpgrador()->mCantGetMoreUsers = true;
		ModuleFileManager::RemoveFile(filenamenext_token.c_str());
		TwitterConnect::randomizeVector(v);
	}

	KigsCore::Disconnect(mTwitterConnect.get(), "FollowRetrieved", this, "manageRetrievedFollow");
	TwitterConnect::SaveFollowFile(GetUpgrador()->mForID, v, GetUpgrador()->mFollowtype);

	requestDone();
}

void	CoreFSMStateClassMethods(TwitterAnalyser, GetFollow)::copyUserList(TwitterAnalyser::UserList& touserlist)
{
	touserlist = std::move(GetUpgrador()->mUserlist);
}

void	CoreFSMStartMethod(TwitterAnalyser, GetRetweeters)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetRetweeters)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetRetweeters))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}
	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(GetUpgrador()->mForID) + "_RetweeterNextCursor.json";

	CoreItemSP nextt = TwitterConnect::LoadJSon(filenamenext_token);

	std::string next_cursor = "-1";

	std::vector<u64>	 v;
	bool found=false;
	if (nextt)
	{
		next_cursor = nextt["next-cursor"]->toString();
	}
	else
	{
		found = TwitterConnect::LoadRetweetersFile(GetUpgrador()->mForID,v);
	}

	if ((!found) || (next_cursor != "-1"))
	{
		// warning ! same callback as likers => signal is LikersRetrieved
		KigsCore::Connect(mTwitterConnect.get(), "LikersRetrieved", this, "manageRetrievedRetweeters");
		mTwitterConnect->launchGetRetweeters(GetUpgrador()->mForID, next_cursor);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else
	{
		GetUpgrador()->mUserlist.clear();
		GetUpgrador()->mUserlist.addUsers(v);
		GetUpgrador()->popState();
	}

	return false;
}


void	CoreFSMStateClassMethods(TwitterAnalyser, GetRetweeters)::manageRetrievedRetweeters(std::vector<u64>& retweeters, const std::string& nexttoken)
{
	u64 tweetID = GetUpgrador()->mForID;

	std::string filenamenext_token = "Cache/Tweets/";
	filenamenext_token += std::to_string(tweetID) + "_RetweeterNextCursor.json";

	std::vector<u64> v;
	TwitterConnect::LoadRetweetersFile(tweetID,v);
	v.insert(v.end(), retweeters.begin(), retweeters.end());

	if (nexttoken != "-1")
	{
		CoreItemSP currentUserJson = MakeCoreMap();
		currentUserJson->set("next-cursor", nexttoken);
		TwitterConnect::SaveJSon(filenamenext_token, currentUserJson);
	}
	else
	{
		ModuleFileManager::RemoveFile(filenamenext_token.c_str());
		TwitterConnect::randomizeVector(v);
	}

	// warning ! same callback as likers => signal is LikersRetrieved
	KigsCore::Disconnect(mTwitterConnect.get(), "LikersRetrieved", this, "manageRetrievedRetweeters");
	TwitterConnect::SaveRetweetersFile(v, tweetID);

	requestDone();
}


void	CoreFSMStartMethod(TwitterAnalyser, RetrieveTweets)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, RetrieveTweets)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, RetrieveTweets))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	std::string username = GetUpgrador()->mUserName;

	if (GetUpgrador()->mUseHashtag)
	{
		username = TwitterConnect::getHashtagFilename(username);
	}

	SP<CoreFSM> fsm = mFsm;
	auto getTweetsState = getFSMState(fsm, TwitterAnalyser, GetTweets);

	if (getTweetsState->mTweets.size() || getTweetsState->mCantGetMoreTweets)
	{
		GetUpgrador()->mTweets = getTweetsState->mTweets;
		if (!GetUpgrador()->mUseHashtag) // filter tweet 
		{
			TwitterConnect::FilterTweets(GetUpgrador()->mUserID, GetUpgrador()->mTweets, GetUpgrador()->mExcludeRetweets, GetUpgrador()->mExcludeReplies);
		}

		bool askmoretweets = true;
		// limit tweets in time
		if(GetUpgrador()->mTimeLimit)
		{
			auto currentTime = time(0);

			u64 timelimitinseconds = GetUpgrador()->mTimeLimit * 24 * 60 * 60;

			size_t twtindex = 0;
			size_t offlimit = 0;
			for (const auto& twt : GetUpgrador()->mTweets)
			{
				time_t cr = twt.getCreationDate();

				double diffseconds = difftime(currentTime, cr);
				if (diffseconds > timelimitinseconds)
				{
					offlimit++;
					if (offlimit >= 3)
					{
						askmoretweets = false;
						break;
					}
				}
				twtindex++;
			}

			GetUpgrador()->mTweets.resize(twtindex);
		}

		if (getTweetsState->mNeededTweetCount && (getTweetsState->mNeededTweetCount < getTweetsState->mTweets.size()))
		{
			getTweetsState->mNeededTweetCount = getTweetsState->mTweets.size();
		}

		if ( (getTweetsState->mCantGetMoreTweets) || (!askmoretweets))
		{
			GetUpgrador()->mCanGetMore = false;
			GetUpgrador()->popState();
			// clean get tweet state
			getTweetsState->mCantGetMoreTweets = false;
			getTweetsState->mTweets.clear();
			return false;
		}
		else
		{
			GetUpgrador()->mCanGetMore = true;
			if (GetUpgrador()->mAskMore)
			{
				GetUpgrador()->mAskMore = false;
				if(getTweetsState->mNeededTweetCount)
					getTweetsState->mNeededTweetCount += getTweetsState->mNeededTweetCountIncrement;
			}
			else
			{
				GetUpgrador()->popState();
				return false;
			}
		}
		
	}

	getTweetsState->mUserName = username;

	if (GetUpgrador()->mUseHashtag)
	{
		getTweetsState->mSearchTweets = true;
		getTweetsState->mHashTag = GetUpgrador()->mUserName;
	}
	else
	{
		getTweetsState->mUserID = GetUpgrador()->mUserID;
		getTweetsState->mSearchTweets = false;
	}

	GetUpgrador()->activateTransition("gettweetstransition");

	return false;
}




void	CoreFSMStartMethod(TwitterAnalyser, UpdateStats)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, UpdateStats)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, UpdateStats))
{
	auto userlist = mPanelUserList.getList();

	auto user = userlist[mCurrentTreatedPanelUserIndex].first;

	if (!TwitterConnect::LoadUserStruct(user, mPanelRetreivedUsers.getUserStruct(user), false))
	{
		askUserDetail(user);
	}

	const auto& currentData = GetUpgrador()->mUserlist.getList();

	if (currentData.size() || GetUpgrador()->mIsValid)
	{
		auto& currentUserData = mPerPanelUsersStats[user];

		for (auto f : currentData)
		{
			currentUserData.addUser(f);

			auto alreadyfound = mInStatsUsers.find(f.first);
			if (alreadyfound != mInStatsUsers.end())
			{
				(*alreadyfound).second.first++;
			}
			else
			{
				TwitterConnect::UserStruct	toAdd;

				mInStatsUsers[f.first] = std::pair<unsigned int, TwitterConnect::UserStruct>(1, toAdd);
			}
		}

		// this one is done
		mValidUserCount++;
	}
	mCurrentTreatedPanelUserIndex++;
	mTreatedUserCount++;

	GetUpgrador()->popState();

	return false;
}



void	CoreFSMStartMethod(TwitterAnalyser, GetPosters)
{

}
void	CoreFSMStopMethod(TwitterAnalyser, GetPosters)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TwitterAnalyser, GetPosters))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	GetUpgrador()->mUserlist.clear();
	GetUpgrador()->mUserlist.addUser(GetUpgrador()->mForID);
	
	// one poster per tweet
	GetUpgrador()->mCantGetMoreUsers = true;

	GetUpgrador()->popState();


	return false;
}