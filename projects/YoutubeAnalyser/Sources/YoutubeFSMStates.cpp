#include "YoutubeFSMStates.h"


using namespace Kigs;
using namespace Kigs::Fsm;
using namespace Kigs::File;

void CoreFSMStartMethod(YoutubeAnalyser, InitChannel)
{

}

void CoreFSMStopMethod(YoutubeAnalyser, InitChannel)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, InitChannel))
{

	if (!YoutubeConnect::LoadChannelID(mChannelName, mChannelID)) // new channel, launch getChannelID
	{
		KigsCore::Connect(mYoutubeConnect.get(), "ChannelIDRetrieved", this, "mainChannelID");
		mYoutubeConnect->launchGetChannelID(mChannelName);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else // load current channel
	{
		if (!YoutubeConnect::LoadChannelStruct(mChannelID, mChannelInfos, true))
		{
			KigsCore::Connect(mYoutubeConnect.get(), "ChannelStatsRetrieved", this, "getChannelStats");
			mYoutubeConnect->launchGetChannelStats(mChannelID);
			GetUpgrador()->activateTransition("waittransition");
			mNeedWait = true;
		}
		else
		{
			// active transition
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
	}

	return false;
}

void	CoreFSMStartMethod(YoutubeAnalyser, Wait)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, Wait)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, Wait))
{

	return false;
}

void	CoreFSMStartMethod(YoutubeAnalyser, BaseStepState)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, BaseStepState)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, BaseStepState))
{

	return false;
}


void	CoreFSMStartMethod(YoutubeAnalyser, Done)
{
	mGraphDrawer->setEverythingDone();

	SaveStatFile();
}
void	CoreFSMStopMethod(YoutubeAnalyser, Done)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, Done))
{

	return false;
}

void	CoreFSMStartMethod(YoutubeAnalyser, Error)
{
	mMainInterface["thumbnail"]["ChannelName"]("Text") = "Quota exceeded - close the application and launch again later";
}
void	CoreFSMStopMethod(YoutubeAnalyser, Error)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, Error))
{

	return false;
}



void	CoreFSMStartMethod(YoutubeAnalyser, RetrieveVideo)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, RetrieveVideo)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, RetrieveVideo))
{

	auto thisUpgrador = GetUpgrador();

	// if an active transition already exist, just activate it
	if (thisUpgrador->hasActiveTransition(this))
	{
		return false;
	}

	SP<CoreFSM> fsm = mFsm;
	auto video = ((CoreFSMStateClass(YoutubeAnalyser, GetVideo)*)fsm->getState("GetVideo"));

	if (thisUpgrador->mStateStep == 0)
	{
		thisUpgrador->mStateStep = 1;
		thisUpgrador->activateTransition("getvideotransition");
	}
	else if (thisUpgrador->mStateStep == 1)
	{
		if ( mVideoListToProcess.size() == mCurrentProcessedVideo )
		{
			if (video->mNeedMoreData)  // can't find more video to process
			{
				// go to done state
				thisUpgrador->activateTransition("donetransition");
			}
			else
			{
				video->mNeedMoreData = true; // ask more video
				thisUpgrador->mStateStep = 0;
			}
		}
		else
		{
			thisUpgrador->activateTransition("processvideotransition");
		}
	}

	return false;
}

void	CoreFSMStartMethod(YoutubeAnalyser, GetVideo)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, GetVideo)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, GetVideo))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}

	CoreItemSP initP = YoutubeConnect::LoadVideoList(mChannelID);

	std::string nextPageToken;
	if (GetUpgrador()->mNeedMoreData && initP)
	{
		if (initP["nextPageToken"])
		{
			nextPageToken = "&pageToken=" + (std::string)initP["nextPageToken"];
		}
	}

	if (!initP || (nextPageToken != ""))
	{

		KigsCore::Connect(mYoutubeConnect.get(), "videoRetrieved", this, "manageRetrievedVideo");
		mYoutubeConnect->launchGetVideo(mChannelID, nextPageToken);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;

	}
	else // get video list from file
	{
		CoreItemSP videoList = initP["videos"];
		CoreItemSP videotitles = initP["videosTitles"];
		if (videoList)
		{
			mVideoListToProcess.clear();
			for (int i = 0; i < videoList->size(); i++)
			{
				mVideoListToProcess.push_back({ videoList[i], "",videotitles[i]});
			}
		}
		// pop current state
		GetUpgrador()->popState();
	}
	return false;
}


void	CoreFSMStateClassMethods(YoutubeAnalyser, GetVideo)::manageRetrievedVideo(const std::vector< YoutubeConnect::videoStruct>& videolist, const std::string& nextPage)
{
	std::vector< YoutubeConnect::videoStruct> filterVideoList;

	filterVideoList = mVideoListToProcess;

	for (const auto& v : videolist)
	{
		if ((v.mChannelID != mChannelID) || (mUseKeyword == ""))
		{
			filterVideoList.push_back(v);
		}
	}

	KigsCore::Disconnect(mYoutubeConnect.get(), "videoRetrieved", this, "manageRetrievedVideo");

	YoutubeConnect::SaveVideoList(mChannelID, filterVideoList, nextPage);

	// no more video for now
	GetUpgrador()->mNeedMoreData = false;

	requestDone(); // pop wait state

}


void	CoreFSMStartMethod(YoutubeAnalyser, ProcessVideo)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, ProcessVideo)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, ProcessVideo))
{

	auto thisUpgrador = GetUpgrador();

	// if an active transition already exist, just activate it
	if (thisUpgrador->hasActiveTransition(this))
	{
		return false;
	}

	// need to retreive more videos ?
	if (mVideoListToProcess.size() == mCurrentProcessedVideo)
	{
		thisUpgrador->mStateStep = 0;
		thisUpgrador->popState();
		return false;
	}

	if (thisUpgrador->mStateStep == 0) // start new video management
	{
		thisUpgrador->mVideoID = mVideoListToProcess[mCurrentProcessedVideo].mID;
		// set current video title 
		mMainInterface["CurrentVideo"]("Text") = mVideoListToProcess[mCurrentProcessedVideo].mName;
		thisUpgrador->mAuthorList.clear();
		CoreItemSP initP = YoutubeConnect::LoadCommentList(mChannelID, thisUpgrador->mVideoID);
		if (initP)
		{
			CoreItemSP authorList = initP["authors"];
			if (authorList)
			{
				for (int i = 0; i < authorList->size(); i++)
				{
					thisUpgrador->mAuthorList.insert(authorList[i]);
				}
			}
		}

		thisUpgrador->mStateStep = 1;
	}
	else if(thisUpgrador->mStateStep == 1) // check if more comments are available
	{
		CoreItemSP initP = YoutubeConnect::LoadCommentList(mChannelID, thisUpgrador->mVideoID);

		std::string nextPageToken;
		if (initP)
		{
			if (initP["nextPageToken"])
			{
				nextPageToken = initP["nextPageToken"];
			}
		}
		if (!initP || nextPageToken != "") // get all comments (and all possible authors)
		{
			KigsCore::Connect(mYoutubeConnect.get(), "commentRetrieved", this, "manageRetrievedComment");
			mYoutubeConnect->launchGetComments(mChannelID, thisUpgrador->mVideoID, nextPageToken);
			GetUpgrador()->activateTransition("waittransition");
			mNeedWait = true;	
		}
		else
		{
			if (thisUpgrador->mAuthorList.size()) // if some authors were found, goto step 2
			{
				thisUpgrador->mStateStep = 2;
			}
			else // no author to treat for this video 
			{
				mCurrentProcessedVideo++;
				thisUpgrador->mStateStep = 0;
			}
		}
	}
	else if(thisUpgrador->mStateStep == 2) // now treat authors
	{
		SP<CoreFSM> fsm = mFsm;
		auto treatAuthors = ((CoreFSMStateClass(YoutubeAnalyser, TreatAuthors)*)fsm->getState("TreatAuthors"));

		treatAuthors->mAuthorList.clear();
		treatAuthors->mCurrentTreatedAuthor = 0;
		treatAuthors->mValidAuthorCount = 0;
		for (const auto& v : thisUpgrador->mAuthorList)
		{
			treatAuthors->mAuthorList.push_back(v);
		}

		thisUpgrador->activateTransition("treatauthorstransition");

		mCurrentProcessedVideo++;
		thisUpgrador->mStateStep = 0; // once authors where treated, go to next video

	}


	return false;
}

void	CoreFSMStateClassMethods(YoutubeAnalyser, ProcessVideo)::manageRetrievedComment(const std::set<std::string>& commentlist, const std::string& nextPage)
{
	auto thisUpgrador = GetUpgrador();
	for (const auto& v : commentlist)
	{
		thisUpgrador->mAuthorList.insert(v);
	}

	// copy to vector
	std::vector<std::string>	authors;
	for (const auto& v : thisUpgrador->mAuthorList)
	{
		authors.push_back(v);
	}

	// randomize before save
	YoutubeConnect::randomizeVector(authors);

	KigsCore::Disconnect(mYoutubeConnect.get(), "commentRetrieved", this, "manageRetrievedComment");

	YoutubeConnect::SaveCommentList(mChannelID, GetUpgrador()->mVideoID, authors, nextPage);

	requestDone(); // pop wait state

}



void	CoreFSMStartMethod(YoutubeAnalyser, TreatAuthors)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, TreatAuthors)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, TreatAuthors))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}
	auto thisUpgrador = GetUpgrador();

	if (	(thisUpgrador->mAuthorList.size() == thisUpgrador->mCurrentTreatedAuthor) // no more author to treat for this video
		||	(mMaxUserPerVideo && (thisUpgrador->mValidAuthorCount >= mMaxUserPerVideo)) ) // or max user per video reached
	{
		thisUpgrador->popState();
		return false;
	}

	SP<CoreFSM> fsm = mFsm;
	auto getAuthorInfos = ((CoreFSMStateClass(YoutubeAnalyser, GetAuthorInfos)*)fsm->getState("GetAuthorInfos"));

	if (thisUpgrador->mStateStep == 0) // init author treatment
	{
		getAuthorInfos->mAuthorID = thisUpgrador->mAuthorList[thisUpgrador->mCurrentTreatedAuthor];
		getAuthorInfos->mIsValidAuthor = false; // reinit
		thisUpgrador->mStateStep = 1;
	}
	else if (thisUpgrador->mStateStep == 1) // check this author was not already treated
	{
		// check if not already treated
		if (mAlreadyTreatedAuthors.find(getAuthorInfos->mAuthorID) != mAlreadyTreatedAuthors.end())
		{
			// go to next
			thisUpgrador->mCurrentTreatedAuthor++;
			thisUpgrador->mStateStep = 0;
		}
		else
		{
			thisUpgrador->mStateStep = 2;
		}
	}
	else if (thisUpgrador->mStateStep == 2) // get author informations
	{
		thisUpgrador->activateTransition("getauthorinfostransition");
		thisUpgrador->mStateStep = 3;
	}
	else if (thisUpgrador->mStateStep == 3) // this author is done, go to next one
	{
		mAlreadyTreatedAuthors.insert(getAuthorInfos->mAuthorID);
		if (getAuthorInfos->mIsValidAuthor)
		{
			thisUpgrador->mValidAuthorCount++;

			auto& currentUserData = mPerPanelUsersStats[getAuthorInfos->mAuthorID];

			for (const auto& f : getAuthorInfos->mCurrentUser.mPublicChannels)
			{
				currentUserData.addUser(f);
			}


		}
		thisUpgrador->mCurrentTreatedAuthor++;
		thisUpgrador->mStateStep = 0;
	}
	return false;
}

void	CoreFSMStartMethod(YoutubeAnalyser, GetAuthorInfos)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, GetAuthorInfos)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, GetAuthorInfos))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}
	auto thisUpgrador = GetUpgrador();

	SP<CoreFSM> fsm = mFsm;
	auto retrievesubscstate = ((CoreFSMStateClass(YoutubeAnalyser, RetrieveSubscriptions)*)fsm->getState("RetrieveSubscriptions"));

	if (thisUpgrador->mStateStep == 0) // init author subscription 
	{
		CoreItemSP initP = YoutubeConnect::LoadAuthorFile(thisUpgrador->mAuthorID);
		if (initP)
		{
			CoreItemSP channels = initP["channels"];
			thisUpgrador->mIsValidAuthor = false;
			thisUpgrador->mCurrentUser.mHasSubscribed = false;
			thisUpgrador->mCurrentUser.mPublicChannels.clear();

			if (channels)
			{
				thisUpgrador->mIsValidAuthor = true;
				myPublicWriters++;
				for (int i = 0; i < channels->size(); i++)
				{
					std::string chanID = channels[i];

					if (chanID == mChannelID) // ok this user subscribes to analysed channel
					{
						thisUpgrador->mCurrentUser.mHasSubscribed = true;
					}

					thisUpgrador->mCurrentUser.mPublicChannels.push_back(chanID);
				}
			}
			if (initP["nextPageToken"]) // if more subscriptions available, get them
			{
				thisUpgrador->mStateStep = 1;
				retrievesubscstate->mAuthorID = thisUpgrador->mAuthorID;
				retrievesubscstate->mSubscriptions = thisUpgrador->mCurrentUser.mPublicChannels;
				retrievesubscstate->mNextPageToken = initP["nextPageToken"];
			}
			else // all subscription were found
			{
				mFoundUsers[thisUpgrador->mAuthorID] = thisUpgrador->mCurrentUser;
				if (thisUpgrador->mIsValidAuthor)
				{
					// add subscribed channels to map
					for (const auto& c : thisUpgrador->mCurrentUser.mPublicChannels)
					{
						// add new found channel
						auto found = mFoundChannels.find(c);
						if (found == mFoundChannels.end())
						{
							YoutubeConnect::ChannelStruct* toAdd = new YoutubeConnect::ChannelStruct();
							toAdd->mID = c;
							toAdd->mName = "";
							toAdd->mThumb.mTexture = nullptr;
							toAdd->mThumb.mURL = "";
							toAdd->mTotalSubscribers = 0;
							toAdd->mViewCount = 0;

							mFoundChannels[c] = toAdd;
							if (thisUpgrador->mCurrentUser.mHasSubscribed)
							{
								toAdd->mSubscribersCount = 1;
								toAdd->mNotSubscribedSubscribersCount = 0;
							}
							else
							{
								toAdd->mSubscribersCount = 0;
								toAdd->mNotSubscribedSubscribersCount = 1;
							}
						}
						else
						{
							if (thisUpgrador->mCurrentUser.mHasSubscribed)
							{
								(*found).second->mSubscribersCount++;
							}
							else
							{
								(*found).second->mNotSubscribedSubscribersCount++;
							}
						}
					}


					thisUpgrador->mStateStep = 2; // get channel infos
				}
				else
				{
					thisUpgrador->mStateStep = 0; 
					thisUpgrador->popState(); // done
				}
			}
		}
		else // need to retrieve subscription
		{
			thisUpgrador->mStateStep = 1;
			retrievesubscstate->mAuthorID = thisUpgrador->mAuthorID;
			retrievesubscstate->mSubscriptions.clear();
			retrievesubscstate->mNextPageToken = "";
		}
	}
	else if (thisUpgrador->mStateStep == 1) // retrieve subscription
	{
		// retrieve user subscription
		thisUpgrador->activateTransition("retrievesubscriptiontransition");

		// once retrieved, go back to step 0 to load the file
		thisUpgrador->mStateStep = 0;
	}
	else if (thisUpgrador->mStateStep == 2) // get infos for this author (only if author is valid <=> has public subscription)
	{
		
		// do we already have infos ?
		YoutubeConnect::ChannelStruct	loaded;
		if (YoutubeConnect::LoadChannelStruct(thisUpgrador->mAuthorID, loaded, false))
		{
			if (thisUpgrador->mCurrentUser.mHasSubscribed) // add this author to subscriber list
			{
				mSubscribedAuthorInfos[thisUpgrador->mAuthorID] = loaded; 
			}
			else // add this author to not subscriber list
			{
				mNotSubscribedAuthorInfos[thisUpgrador->mAuthorID] = loaded;
			}

			// ok we are good with this author
			thisUpgrador->mStateStep = 0;
			thisUpgrador->popState();
		}
		else
		{
			thisUpgrador->mStateStep = 3;
		}
	
	}
	else if (thisUpgrador->mStateStep == 3) // retrieve informations for this author
	{
		KigsCore::Connect(mYoutubeConnect.get(), "ChannelStatsRetrieved", this, "getChannelStats");
		mYoutubeConnect->launchGetChannelStats(thisUpgrador->mAuthorID);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
		// go back to step 2
		thisUpgrador->mStateStep = 2;
	}

	return false;
}


void	CoreFSMStartMethod(YoutubeAnalyser, RetrieveSubscriptions)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, RetrieveSubscriptions)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, RetrieveSubscriptions))
{
	// if an active transition already exist, just activate it
	if (GetUpgrador()->hasActiveTransition(this))
	{
		return false;
	}
	auto thisUpgrador = GetUpgrador();

	if (thisUpgrador->mStateStep == 0) // retrieve subscription, do it again until mNextPageToken is empty
	{
		KigsCore::Connect(mYoutubeConnect.get(), "subscriptionRetrieved", this, "manageRetrievedSubscriptions");
		mYoutubeConnect->launchGetSubscriptions(thisUpgrador->mAuthorID, thisUpgrador->mNextPageToken);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	else if (thisUpgrador->mStateStep == 1) // no more subscription to get
	{
		thisUpgrador->mStateStep = 0;
		thisUpgrador->mNextPageToken = "";
		thisUpgrador->popState();
	}

	return false;
}


void	CoreFSMStateClassMethods(YoutubeAnalyser, RetrieveSubscriptions)::manageRetrievedSubscriptions(const std::vector<std::string>& subscrlist, const std::string& nextPage)
{
	auto thisUpgrador = GetUpgrador();
	
	for (auto& chan : subscrlist)
	{
		thisUpgrador->mSubscriptions.push_back(chan);
	}

	thisUpgrador->mNextPageToken = nextPage;

	KigsCore::Disconnect(mYoutubeConnect.get(), "subscriptionRetrieved", this, "manageRetrievedSubscriptions");

	if (nextPage == "")
	{
		// randomize before save
		YoutubeConnect::randomizeVector(thisUpgrador->mSubscriptions);
		thisUpgrador->mStateStep = 1;
	}

	YoutubeConnect::SaveAuthorFile(GetUpgrador()->mAuthorID, thisUpgrador->mSubscriptions, nextPage);

	requestDone(); // pop wait state

}


void	CoreFSMStartMethod(YoutubeAnalyser, GetUserListDetail)
{

}
void	CoreFSMStopMethod(YoutubeAnalyser, GetUserListDetail)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(YoutubeAnalyser, GetUserListDetail))
{
	if (!mUserDetailsAsked.size())
	{
		mNeedUserListDetail = false;
		GetUpgrador()->popState();
		return false;
	}

	const std::string& userID = mUserDetailsAsked.back();

	if (!YoutubeConnect::LoadChannelStruct(userID, GetUpgrador()->mTmpUserStruct, false))
	{
		KigsCore::Connect(mYoutubeConnect.get(), "ChannelStatsRetrieved", this, "getChannelStats");
		mYoutubeConnect->launchGetChannelStats(userID);
		GetUpgrador()->activateTransition("waittransition");
		mNeedWait = true;
	}
	mUserDetailsAsked.pop_back();

	return false;
}

