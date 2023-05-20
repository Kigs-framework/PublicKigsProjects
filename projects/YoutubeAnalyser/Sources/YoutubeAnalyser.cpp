#include "YoutubeAnalyser.h"
#include "FilePathManager.h"
#include "NotificationCenter.h"
#include "HTTPRequestModule.h"
#include "HTTPConnect.h"
#include "JSonFileParser.h"
#include "ResourceDownloader.h"
#include "TinyImage.h"
#include "JPEGClass.h"
#include "PNGClass.h"
#include "UI/UIImage.h"
#include "TextureFileManager.h"
#include "Histogram.h"
#include "CoreFSM.h"
#include "YoutubeFSMStates.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

using namespace Kigs;
using namespace Kigs::Http;
using namespace Kigs::File;
using namespace Kigs::Draw2D;

//#define LOG_ALL
#ifdef LOG_ALL
SmartPointer<::FileHandle> LOG_File = nullptr;

void	log(std::string logline)
{
	if (LOG_File->myFile)
	{
		logline += "\n";
		Platform_fwrite(logline.c_str(), 1, logline.length(), LOG_File.get());
	}
}

void closeLog()
{
	Platform_fclose(LOG_File.get());
}

#endif


void	YoutubeAnalyser::createFSMStates()
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
	
	// init state
	fsm->addState("Init", new CoreFSMStateClass(YoutubeAnalyser, InitChannel)());


	fsm->getState("Init")->addTransition(mTransitionList["waittransition"]);

	// pop wait state transition
	SP<CoreFSMTransition> waitendtransition = KigsCore::GetInstanceOf("waitendtransition", "CoreFSMOnValueTransition");
	waitendtransition->setValue("TransitionBehavior", "Pop");
	waitendtransition->setValue("ValueName", "NeedWait");
	waitendtransition->setValue("NotValue", true); // end wait when NeedWait is false
	waitendtransition->Init();

	mTransitionList["waitendtransition"] = waitendtransition;

	// create Wait state
	fsm->addState("Wait", new CoreFSMStateClass(YoutubeAnalyser, Wait)());
	// Wait state can pop back to previous state
	fsm->getState("Wait")->addTransition(waitendtransition);

	// this one is needed for all cases
	fsm->addState("Done", new CoreFSMStateClass(YoutubeAnalyser, Done)());

	// transition to done state when checkDone returns true
	SP<CoreFSMTransition> donetransition = KigsCore::GetInstanceOf("donetransition", "CoreFSMOnMethodTransition");
	donetransition->setState("Done");
	donetransition->setValue("MethodName", "checkDone");
	donetransition->Init();

	mTransitionList["donetransition"] = donetransition;


	// go to RetrieveVideo
	SP<CoreFSMTransition> retrievevideotransition = KigsCore::GetInstanceOf("retrievevideotransition", "CoreFSMInternalSetTransition");
	retrievevideotransition->setState("RetrieveVideo");
	retrievevideotransition->Init();

	// create GetVideo transition (Push)
	SP<CoreFSMTransition> getvideotransition = KigsCore::GetInstanceOf("getvideotransition", "CoreFSMInternalSetTransition");
	getvideotransition->setValue("TransitionBehavior", "Push");
	getvideotransition->setState("GetVideo");
	getvideotransition->Init();

	// create ProcessVideo transition (Push)
	SP<CoreFSMTransition> processvideotransition = KigsCore::GetInstanceOf("processvideotransition", "CoreFSMInternalSetTransition");
	processvideotransition->setValue("TransitionBehavior", "Push");
	processvideotransition->setState("ProcessVideo");
	processvideotransition->Init();

	fsm->getState("Init")->addTransition(retrievevideotransition);

	// create RetrieveVideo state
	fsm->addState("RetrieveVideo", new CoreFSMStateClass(YoutubeAnalyser, RetrieveVideo)());
	fsm->getState("RetrieveVideo")->addTransition(getvideotransition);
	fsm->getState("RetrieveVideo")->addTransition(processvideotransition);
	fsm->getState("RetrieveVideo")->addTransition(mTransitionList["donetransition"]);

	// create GetVideo state
	fsm->addState("GetVideo", new CoreFSMStateClass(YoutubeAnalyser, GetVideo)());
	fsm->getState("GetVideo")->addTransition(mTransitionList["waittransition"]);


	// create GetComment transition (Push)
	SP<CoreFSMTransition> getcommenttransition = KigsCore::GetInstanceOf("getcommenttransition", "CoreFSMInternalSetTransition");
	getcommenttransition->setValue("TransitionBehavior", "Push");
	getcommenttransition->setState("GetComment");
	getcommenttransition->Init();

	// create TreatAuthors transition (Push)
	SP<CoreFSMTransition> treatauthorstransition = KigsCore::GetInstanceOf("treatauthorstransition", "CoreFSMInternalSetTransition");
	treatauthorstransition->setValue("TransitionBehavior", "Push");
	treatauthorstransition->setState("TreatAuthors");
	treatauthorstransition->Init();

	// create ProcessVideo state
	fsm->addState("ProcessVideo", new CoreFSMStateClass(YoutubeAnalyser, ProcessVideo)());
	fsm->getState("ProcessVideo")->addTransition(getcommenttransition);
	fsm->getState("ProcessVideo")->addTransition(treatauthorstransition);
	fsm->getState("ProcessVideo")->addTransition(mTransitionList["donetransition"]);
	fsm->getState("ProcessVideo")->addTransition(mTransitionList["waittransition"]);


	// create GetAuthorInfos transition (Push)
	SP<CoreFSMTransition> getauthorinfostransition = KigsCore::GetInstanceOf("getauthorinfostransition", "CoreFSMInternalSetTransition");
	getauthorinfostransition->setValue("TransitionBehavior", "Push");
	getauthorinfostransition->setState("GetAuthorInfos");
	getauthorinfostransition->Init();

	// create TreatAuthors state
	fsm->addState("TreatAuthors", new CoreFSMStateClass(YoutubeAnalyser, TreatAuthors)());
	fsm->getState("TreatAuthors")->addTransition(getauthorinfostransition);
	fsm->getState("TreatAuthors")->addTransition(mTransitionList["waittransition"]);


	// create RetrieveSubscriptions transition (Push)
	SP<CoreFSMTransition> retrievesubscriptiontransition = KigsCore::GetInstanceOf("retrievesubscriptiontransition", "CoreFSMInternalSetTransition");
	retrievesubscriptiontransition->setValue("TransitionBehavior", "Push");
	retrievesubscriptiontransition->setState("RetrieveSubscriptions");
	retrievesubscriptiontransition->Init();

	// create GetAuthorInfos state
	fsm->addState("GetAuthorInfos", new CoreFSMStateClass(YoutubeAnalyser, GetAuthorInfos)());
	fsm->getState("GetAuthorInfos")->addTransition(retrievesubscriptiontransition);
	fsm->getState("GetAuthorInfos")->addTransition(mTransitionList["waittransition"]);


	// create RetrieveSubscriptions state
	fsm->addState("RetrieveSubscriptions", new CoreFSMStateClass(YoutubeAnalyser, RetrieveSubscriptions)());
	fsm->getState("RetrieveSubscriptions")->addTransition(mTransitionList["waittransition"]);

	

	/*

	// this one is needed for all cases
	fsm->addState("GetUserListDetail", new CoreFSMStateClass(YoutubeAnalyser, GetUserListDetail)());
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
	fsm->addState("Done", new CoreFSMStateClass(YoutubeAnalyser, Done)());
	// only wait or pop
	fsm->getState("Done")->addTransition(userlistdetailtransition);



	// transition to done state when checkDone returns true
	SP<CoreFSMTransition> donetransition = KigsCore::GetInstanceOf("donetransition", "CoreFSMOnMethodTransition");
	donetransition->setState("Done");
	donetransition->setValue("MethodName", "checkDone");
	donetransition->Init();

	mTransitionList["donetransition"] = donetransition;*/

}

IMPLEMENT_CLASS_INFO(YoutubeAnalyser);

IMPLEMENT_CONSTRUCTOR(YoutubeAnalyser)
{

}

void	replaceSpacesByEscapedOr(std::string& keyw)
{
	size_t pos = keyw.find(" ");
	while (pos != std::string::npos)
	{
		keyw.replace(pos, 1, "%7C");
		pos = keyw.find(" ");
	}
}

void	YoutubeAnalyser::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	mCurrentTime = time(0);

	// TODO : get this in parameter file
	// here don't use files olders than three months.
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;

	SP<FilePathManager> pathManager = KigsCore::Singleton<FilePathManager>();
	pathManager->AddToPath(".", "xml");
	// when a path is given, search the file only with this path
	pathManager->setValue("StrictPath", true);
	pathManager->AddToPath(".", "json");

	CoreCreateModule(HTTPRequestModule, 0);

	// Init App
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary("launchParams.json");

	// generic youtube management class
	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), YoutubeConnect, YoutubeConnect, YoutubeAnalyser);
	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), GraphDrawer, GraphDrawer, YoutubeAnalyser);

	mYoutubeConnect = KigsCore::GetInstanceOf("mYoutubeConnect", "YoutubeConnect");
	mYoutubeConnect->initBearer(initP);

	mGraphDrawer = KigsCore::GetInstanceOf("mGraphDrawer", "GraphDrawer");

	// retreive parameters
	mChannelName = initP["ChannelID"];

	auto SetMemberFromParam = [&]<typename T>(T & x, const char* id) {
		if (initP[id]) x = initP[id].value<T>();
	};
	
	SetMemberFromParam(mSubscribedUserCount, "ValidUserCount");
	SetMemberFromParam(mMaxChannelCount, "MaxChannelCount");
	SetMemberFromParam(mValidChannelPercent, "ValidChannelPercent");
	SetMemberFromParam(mMaxUserPerVideo, "MaxUserPerVideo");
	SetMemberFromParam(mUseKeyword, "UseKeyword");

	SetMemberFromParam(mFromDate, "FromDate");
	SetMemberFromParam(mToDate, "ToDate");

	replaceSpacesByEscapedOr(mUseKeyword);

	int oldFileLimitInDays=3*30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");
	
	mOldFileLimit = 60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays;

	mYoutubeConnect->initConnection(mOldFileLimit);

	// connect done msg
	KigsCore::Connect(mYoutubeConnect.get(), "done", this, "requestDone");

	initCoreFSM();

	// add FSM
	SP<CoreFSM> fsm = KigsCore::GetInstanceOf("fsm", "CoreFSM");
	// need to add fsm to the object to control
	addItem(fsm);
	mFsm = fsm;

	createFSMStates();
	
#ifdef LOG_ALL
	LOG_File = Platform_fopen("Log_All.txt", "wb");
#endif



	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
	mLastUpdate = GetApplicationTimer()->GetTime();
}

void	YoutubeAnalyser::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();
	
	return;
	/*
	if (mDrawForceBased)
	{
		DrawForceBased();
		return;
	}

	// everything is done
	if (mState == 50)
	{
		// just refresh graphics
		double dt = GetApplicationTimer()->GetTime() - mLastUpdate;
		if (dt > 1.0)
		{
			mLastUpdate = GetApplicationTimer()->GetTime();
			refreshAllThumbs();
		}
		return;
	}

	bool needMoreData = false;

	// goal was reached ?
	if (mySubscribedWriters >= mSubscribedUserCount)
	{
		if(mState != 50) // finished ?
			mState = 0; // stop updating
	}

	// check state
	switch(mState)
	{
	case 60: // error sequence
	case 0: // wait
		break;
	case 11: // retrieve more videos for this channel
#ifdef LOG_ALL
		log("********************    state 11");
#endif
		needMoreData = true;
	case 1: // process this channel
	{
#ifdef LOG_ALL
		log("********************    state 1");
#endif
		mState = 0; // will wait 
		mNotSubscribedUserForThisVideo = 0;
		mBadVideo = false;
		// search video list in cache

		std::string filename = "Cache/" + mChannelName + "/videos/";
		filename += mChannelID;

		// if date use them
		if (mFromDate.length())
		{
			filename += "_from_" + mFromDate + "_";
		}
		if (mToDate.length())
		{
			filename += "_to_" + mToDate + "_";
		}

		filename += ".json";

		CoreItemSP initP = LoadJSon(filename);
		// need to get more videos than the ones already retrieved ?
		std::string nextPageToken;
		if (needMoreData && initP)
		{
			if (initP["nextPageToken"])
			{
				nextPageToken = "&pageToken=" + (std::string)initP["nextPageToken"];
			}
		}

		if (!initP || (nextPageToken!=""))
		{
			// get latest video list
			// GET https://www.googleapis.com/youtube/v3/search?part=snippet&channelId={CHANNEL_ID}&maxResults=10&order=date&type=video&key={YOUR_API_KEY}
			
			// https://www.googleapis.com/youtube/v3/search?part=snippet&maxResults=25&q=surfing&key={YOUR_API_KEY}
			
			std::string url = "/youtube/v3/search?part=snippet";

			if (mUseKeyword == "")
			{
				url += "&channelId=" + mChannelID;
			}
			else
			{
				url += "&regionCode=FR&relevanceLanguage=fr&q=" + mUseKeyword;
			}
			std::string request = url  + mYoutubeConnect->getCurrentBearer() + nextPageToken + "&maxResults=50&order=date&type=video";
			
			// if date use them
			if (mFromDate.length())
			{
				request += "&publishedAfter=" + mFromDate + "T00:00:00Z";
			}
			if (mToDate.length())
			{
				request += "&publishedBefore=" + mToDate + "T23:59:00Z";
			}

			mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getVideoList", this);
			printf("Request : getVideoList\n ");
			mAnswer->Init();
			myRequestCount++;
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
					mVideoListToProcess.push_back({ videoList[i], videotitles[i] });
				}
				mState = 2; // then go to process videos
			}
			else
			{
				mState = 10; // error  
			}
		}
	}
	break;
	case 21: // retrieve more comments from this video
		needMoreData = true;

#ifdef LOG_ALL
		log("********************    state 21");
#endif
	case 2: // treat video list
	{
#ifdef LOG_ALL
		log("********************    state 2");
#endif
		mState = 0; // wait

		if (mVideoListToProcess.size()> mCurrentProcessedVideo)
		{
			mAuthorListToProcess.clear();
			mCurrentAuthorList.clear();
			// https://www.googleapis.com/youtube/v3/commentThreads?part=snippet%2Creplies&videoId=_VB39Jo8mAQ&key=[YOUR_API_KEY]' 
			std::string videoID = mVideoListToProcess[mCurrentProcessedVideo].first;

			// set current video title 
			mMainInterface["CurrentVideo"]("Text") = mVideoListToProcess[mCurrentProcessedVideo].second;

#ifdef LOG_ALL
			log(std::string("Process video ")+ videoID);
#endif
			std::string filename = "Cache/" + mChannelName + "/videos/";
			filename += videoID + "_videos.json";

			CoreItemSP initP = LoadJSon(filename);

			// check if we can retrieve more comments from the same video
			std::string nextPageToken;
			if (needMoreData && initP)
			{
				if (initP["nextPageToken"])
				{
					nextPageToken = "&pageToken=" + (std::string)initP["nextPageToken"];
				}
				else
				{
					mState = 2;
					mCurrentProcessedVideo++;
					mCurrentVideoUserFound = 0;
					mNotSubscribedUserForThisVideo = 0;
					break;
				}
			}			

			// request new comments
			if (!initP || nextPageToken!="")
			{
				std::string url = "/youtube/v3/commentThreads?part=snippet%2Creplies&videoId=";
				std::string request = url + videoID + mYoutubeConnect->getCurrentBearer() + nextPageToken +  "&maxResults=50&order=time";
				mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getCommentList", this);
				mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "videoID", videoID.c_str());
				mAnswer->Init();
				printf("Request : getCommentList\n ");
				myRequestCount++;
			}
			else // get authors from already saved file
			{
				mParsedComments += (int)initP["ParsedComments"];
				
				CoreItemSP authorList = initP["authors"];
				if (authorList)
				{
					for (int i = 0; i < authorList->size(); i++)
					{
						mAuthorListToProcess.push_back(authorList[i]);
						mCurrentAuthorList.insert(authorList[i]);
					}
					mState = 3;
				}
				else
				{
					mState = 11; // retrieve more videos for this channel
				}

			}
		}
		else
		{
			mState = 11; // retrieve more videos for this channel
		}
	}
	break;
	case 3: // treat author list
	{
#ifdef LOG_ALL
		log("********************    state 3");
#endif
		mState = 0; // wait

		// check if we reached user count per video goal
		if ( (mMaxUserPerVideo && (mCurrentVideoUserFound >= mMaxUserPerVideo)) || (mBadVideo))
		{
#ifdef LOG_ALL
			log("********************* Max user for video");
#endif
			// go to next video
			mState = 2;
			mCurrentProcessedVideo++;
			mCurrentVideoUserFound = 0;
			mNotSubscribedUserForThisVideo = 0;
			mBadVideo = false;
			break;
		}

		if (mAuthorListToProcess.size())
		{

			// https://www.googleapis.com/youtube/v3/subscriptions?part=snippet%2CcontentDetails&channelId=UC_x5XG1OV2P6uZZ5FSM9Ttw&key=[YOUR_API_KEY]'
 
			mCurrentProcessedUser = *mAuthorListToProcess.begin();
			mAuthorListToProcess.erase(mAuthorListToProcess.begin());
#ifdef LOG_ALL
			log(std::string("Process user ") + mCurrentProcessedUser);
#endif
			if (mFoundUsers.find(mCurrentProcessedUser) != mFoundUsers.end())
			{
#ifdef LOG_ALL
				log("!!!!!!!!!!!! already treated user");
#endif

				printf("already treated user\n");
				mState = 3; // process next author
			}
			else
			{
				std::string filename = "Cache/Authors/";
				filename += mCurrentProcessedUser + ".json";

				CoreItemSP initP = LoadJSon(filename);

				// we don't know this writer, retrieve its subscription
				if (!initP)
				{
					std::string url = "/youtube/v3/subscriptions?part=snippet%2CcontentDetails&channelId=";
					std::string request = url + mCurrentProcessedUser + mYoutubeConnect->getCurrentBearer() + "&maxResults=50";
					mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getUserSubscribtion", this);
					mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "request", request.c_str());
					mAnswer->Init();
					printf("Request : getUserSubscribtion\n ");
					myRequestCount++;
				}
				else // load subscriptions from file
				{
					CoreItemSP channels = initP["channels"];
					mCurrentUser.mHasSubscribed = false;
					mCurrentUser.mPublicChannels.clear();
					if (channels)
					{
						myPublicWriters++;
						for (int i = 0; i < channels->size(); i++)
						{
							std::string chanID = channels[i];

							if (chanID == mChannelID) // ok this user subscribes to analysed channel
							{
								mCurrentUser.mHasSubscribed = true;
							}
							else // don't had myChannelID
							{
								mCurrentUser.mPublicChannels.push_back(chanID);
							}
						}
					}

					if (!mCurrentUser.mHasSubscribed)
					{
						if (mCurrentUser.mPublicChannels.size())
						{
							mNotSubscribedUserForThisVideo++;
							// parsed 40 users and none has subscribed to this channel
							// so set bad video flag
							if (mNotSubscribedUserForThisVideo > 20)
							{
								mBadVideo = true;
							}
						}
						for (const auto& c : mCurrentUser.mPublicChannels)
						{
							// add new found channel
							auto found = mFoundChannels.find(c);
							if (found == mFoundChannels.end())
							{
								YoutubeConnect::ChannelStruct* toAdd = new YoutubeConnect::ChannelStruct();
								mFoundChannels[c]=toAdd;
								toAdd->mSubscribersCount = 0;
								toAdd->mNotSubscribedSubscribersCount = 1;
							}
							else
							{
								(*found).second->mNotSubscribedSubscribersCount++;
							}
						}


						mFoundUsers[mCurrentProcessedUser] = mCurrentUser;
					}
					else
					{
						mySubscribedWriters++;
						mNotSubscribedUserForThisVideo = 0;
						mCurrentVideoUserFound++;
						for (const auto& c : mCurrentUser.mPublicChannels)
						{
							// add new found channel
							auto found = mFoundChannels.find(c);
							if (found == mFoundChannels.end())
							{
								YoutubeConnect::ChannelStruct* toAdd = new YoutubeConnect::ChannelStruct();
								mFoundChannels[c] = toAdd;
								toAdd->mSubscribersCount = 1;
								toAdd->mNotSubscribedSubscribersCount = 0;
							}
							else
							{
								(*found).second->mSubscribersCount++;
								if ((*found).second->mSubscribersCount==4)
								{
									if (!LoadChannelStruct(c, *(*found).second))
									{
										// request details
										std::string url = "/youtube/v3/channels?part=statistics,snippet&id=";
										std::string request = url + c + mYoutubeConnect->getCurrentBearer();
										mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
										printf("Request : getChannelStats %s\n ", c.c_str());
										mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::INT, "dontSetState", 0);
										mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.c_str());
										mAnswer->Init();
										myRequestCount++;
									}
								}
							}
						}
						mSubscriberList.push_back(mCurrentProcessedUser);
						mFoundUsers[mCurrentProcessedUser] = mCurrentUser;

					}
					mState = 3; // process next author
				}
			}
		}
		else // go back to comment retrieving
		{
			mState = 21;
			mCurrentAuthorList.clear();
		}
	}
	break;
	case 4: // process each subscribed channel for current user
	{
		mState = 0;
		if (mTmpUserChannels.size())
		{

			auto c = mTmpUserChannels.back();

			mTmpUserChannels.pop_back();

			// add new found channel
			auto found = mFoundChannels.find(c.first);
			if (found == mFoundChannels.end())
			{

				YoutubeConnect::ChannelStruct* toAdd = new YoutubeConnect::ChannelStruct();
				mFoundChannels[c.first] = toAdd;
				if (mCurrentUser.mHasSubscribed)
				{
					toAdd->mSubscribersCount = 1;
					toAdd->mNotSubscribedSubscribersCount = 0;
				}
				else
				{
					toAdd->mSubscribersCount = 0;
					toAdd->mNotSubscribedSubscribersCount = 1;
				}


				mState = 4; // continue mTmpUserChannels
				
			}
			else
			{
				if (mCurrentUser.mHasSubscribed)
				{
					(*found).second->mSubscribersCount++;
					if (!LoadChannelStruct(c.first, *(*found).second))
					{
						(*found).second->mName = c.second.first;
						(*found).second->mThumb.mURL = c.second.second;
						// request details
						std::string url = "/youtube/v3/channels?part=statistics&id=";
						std::string request = url + c.first + mYoutubeConnect->getCurrentBearer();
						mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
						printf("Request : getChannelStats %s\n ", c.first.c_str());
						mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.first.c_str());
						mAnswer->Init();
						myRequestCount++;
						break;
					}
				}
				else
				{
					(*found).second->mNotSubscribedSubscribersCount++;
				}
				mState = 4; // continue mTmpUserChannels
			}
		}
		else
		{
			mState = 3; // process next author
		}

	}
	break;
	case 10:// error
	{
#ifdef LOG_ALL
		log("\nError\n");
#endif
		if (mErrorCode == 403)
		{
			SimpleCall("RequestStateChange", "ErrorSequence.xml");
			mState = 60;
		}
		else if (mErrorCode == 405)
		{
			printf("received empty answer \n");
		}
		else
		{
			// wait no more Async task
			if (!KigsCore::Instance()->hasAsyncRequestPending())
			{
				mShowedChannels.clear();
				mNeedExit = true;
			}
		}
	}
	break;
	default:
#ifdef LOG_ALL
		log("\nunknown state\n");
#endif
		printf("unknown state");
	}


	// update graphics
	double dt = GetApplicationTimer()->GetTime() - mLastUpdate;
	if (dt > 1.0)
	{
		mLastUpdate = GetApplicationTimer()->GetTime();
		if (mMainInterface)
		{
			char textBuffer[256];
			sprintf(textBuffer, "Treated writers : %d", mFoundUsers.size());
			mMainInterface["TreatedWriters"]("Text") = textBuffer;
			sprintf(textBuffer, "Subscribed writers : %d", mySubscribedWriters);
			mMainInterface["SubscWriters"]("Text") = textBuffer;
			if (mFoundUsers.size())
			{
				sprintf(textBuffer, "Public writers : %d%%", (int)((100 * myPublicWriters) / mFoundUsers.size()));
				mMainInterface["PublicWriters"]("Text") = textBuffer;
			}
			sprintf(textBuffer, "Found Channels : %d", mFoundChannels.size());
			mMainInterface["FoundChannels"]("Text") = textBuffer;
			sprintf(textBuffer, "Parsed Comments : %d", mParsedComments);
			mMainInterface["ParsedComments"]("Text") = textBuffer;
			sprintf(textBuffer, "Youtube Data API requests : %d", myRequestCount);
			mMainInterface["RequestCount"]("Text") = textBuffer;

			// check if Channel texture was loaded

			if (mChannelInfos.mThumb.mTexture && mMainInterface["thumbnail"])
			{
				const SP<UIImage>& tmp = mMainInterface["thumbnail"];

				if (!tmp->HasTexture())
				{
					tmp->addItem(mChannelInfos.mThumb.mTexture);
					mMainInterface["thumbnail"]["ChannelName"]("Text") = mChannelInfos.mName;
				}
			}
			else if(mMainInterface["thumbnail"])
			{
				LoadChannelStruct(mChannelID, mChannelInfos,true);
			}

			refreshAllThumbs();
		}
	}
	*/
}

void	YoutubeAnalyser::refreshAllThumbs()
{
/*	if (mySubscribedWriters < 10)
	{
		return;
	}

	bool somethingChanged = false;

	float wantedpercent = mValidChannelPercent;

	if (mySubscribedWriters < mSubscribedUserCount)
	{
		wantedpercent = (mValidChannelPercent * mSubscribedUserCount)/(float)mySubscribedWriters;
	}
	
	// get all parsed channels and get the ones with more than mValidChannelPercent subscribed users
	std::vector<std::pair<YoutubeConnect::ChannelStruct*,std::string>>	toShow;
	for (auto c : mFoundChannels)
	{
		if (c.second->mSubscribersCount > 1)
		{
			float percent = (float)c.second->mSubscribersCount / (float)mySubscribedWriters;
			if (percent > wantedpercent)
			{
				toShow.push_back({ c.second,c.first });
			}
		}
	}

	if (toShow.size() == 0)
	{
		return;
	}

	if (mCurrentMeasure == Similarity) // Jaccard 
	{
		std::sort(toShow.begin(), toShow.end(), [this](const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a1, const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a2)
			{
				if (a1.first->mTotalSubscribers == 0)
				{
					if (a2.first->mTotalSubscribers == 0)
					{
						return a1.second > a2.second;
					}
					return false;
				}
				else if (a2.first->mTotalSubscribers == 0)
				{
					return true;
				}
				// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
				// a1 subscribers %
				float A1_percent = ((float)a1.first->mSubscribersCount / (float)mySubscribedWriters);
				// a1 intersection size with analysed channel
				float A1_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A1_percent;
				// a1 union size with analysed channel 
				float A1_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)a1.first->mTotalSubscribers - A1_a_inter_b;

				// a2 subscribers %
				float A2_percent = ((float)a2.first->mSubscribersCount / (float)mySubscribedWriters);
				// a2 intersection size with analysed channel
				float A2_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A2_percent;
				// a2 union size with analysed channel 
				float A2_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)a2.first->mTotalSubscribers - A2_a_inter_b;

				float k1 = A1_a_inter_b / A1_a_union_b;
				float k2 = A2_a_inter_b / A2_a_union_b;

				if (k1 == k2)
				{
					return a1.second > a2.second;
				}
				return (k1 > k2);
			}
		);
	}
	else
	{
		std::sort(toShow.begin(), toShow.end(), [&](const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a1, const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a2)
			{

				if (mCurrentMeasure != Normalized)
				{
					if (a1.first->mSubscribersCount == a2.first->mSubscribersCount)
					{
						return a1.second > a2.second;
					}
					return (a1.first->mSubscribersCount > a2.first->mSubscribersCount);
				}
				if (a1.first->mTotalSubscribers == 0)
				{
					if (a2.first->mTotalSubscribers == 0)
					{
						return a1.second > a2.second;
					}
					return false;
				}
				else if (a2.first->mTotalSubscribers == 0)
				{
					return true;
				}
				float a1fcount = (a1.first->mTotalSubscribers < 10) ? logf(10.0f) : logf((float)a1.first->mTotalSubscribers);
				float a2fcount = (a2.first->mTotalSubscribers < 10) ? logf(10.0f) : logf((float)a2.first->mTotalSubscribers);

				float A1_w = ((float)a1.first->mSubscribersCount / a1fcount);
				float A2_w = ((float)a2.first->mSubscribersCount / a2fcount);
				if (A1_w == A2_w)
				{
					return a1.second > a2.second;
				}
				return (A1_w > A2_w);
				
			}
		);
	}

	// check for channels already shown / to remove

	std::unordered_map<std::string, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[tos.second] = 1;
		toShowCount++;
		if (toShowCount >= (mMaxChannelCount*3)) // compute 3x max channel count to display cloud
			break;
	}


	for (const auto& s : mShowedChannels)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedChannels.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedChannels.erase(toremove);
			somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_"+ update.first;
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedChannels[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			somethingChanged = true;
		}
	}

	float dangle = 2.0f * fPI / 7.0f;
	float angle = 0.0f;
	float ray = 0.15f;
	float dray = 0.0117f;
	toShowCount = 0;
	float NormalizeSubscriberCountForShown = 1.0f;

	for (const auto& toPlace : toShow)
	{
		auto found = mShowedChannels.find(toPlace.second);
		if (found != mShowedChannels.end())
		{
			NormalizeSubscriberCountForShown = (toPlace.first->mTotalSubscribers < 10) ? logf(10.0f) : logf((float)toPlace.first->mTotalSubscribers);
			break;
		}
	}

	for (const auto& toPlace : toShow)
	{
		auto found=mShowedChannels.find(toPlace.second);
		if (found != mShowedChannels.end())
		{
			
			
			const CMSP& toSetup = (*found).second;
			v2f dock(0.53f + ray * cosf(angle), 0.47f + ray*1.02f * sinf(angle));
			toSetup("Dock") = dock;

			angle += dangle;
			dangle = 2.0f * fPI / (2.0f+50.0f*ray);
			ray += dray;
			dray *= 0.98f;
			toSetup["ChannelName"]("Text") = toPlace.first->mName;
			float prescale = 1.0f;
			if (mCurrentMeasure == Similarity) // Jaccard 
			{
				
				// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
				// A subscribers %
				float A_percent = ((float)toPlace.first->mSubscribersCount / (float)mySubscribedWriters);
				// A intersection size with analysed channel
				float A_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A_percent;
				// A union size with analysed channel 
				float A_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)toPlace.first->mTotalSubscribers - A_a_inter_b;

				float k = 100.0f * A_a_inter_b / A_a_union_b;
				if (k > 100.0f) // avoid Jaccard index greater than 100
				{
					k = 100.0f;
				}
				if (toPlace.first->mTotalSubscribers == 0)
				{
					k = 0.0f;
				}
				toSetup["ChannelPercent"]("Text") = std::to_string((int)(k)) + mUnity[mCurrentMeasure];

				prescale = 1.5f * k / 100.0f;
				
			}
			else
			{
				int percent = (int)(100.0f * ((float)toPlace.first->mSubscribersCount / (float)mySubscribedWriters));

				if (mCurrentMeasure == Normalized)
				{
					float fpercent = (float)toPlace.first->mSubscribersCount / (float)mySubscribedWriters;

					float toplacefcount = (toPlace.first->mTotalSubscribers < 10) ? logf(10.0f) : logf((float)toPlace.first->mTotalSubscribers);
					fpercent *= NormalizeSubscriberCountForShown / toplacefcount;

					percent = (int)(100.0f * fpercent);
				}


				toSetup["ChannelPercent"]("Text") = std::to_string(percent) + mUnity[mCurrentMeasure];

				prescale = percent / 100.0f;
			}

			prescale = sqrtf(prescale);
			if (prescale > 0.8f)
			{
				prescale = 0.8f;
			}
			
			// set ChannnelPercent position depending on where the thumb is in the spiral

			if ((dock.x > 0.45f) && (dock.x < 0.61f))
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.5,0.0);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.5,1.0);
			}
			else if(dock.x<=0.45f)
			{
				toSetup["ChannelPercent"]("Dock") = v2f(1.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.0, 0.5);
			}
			else
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(1.0, 0.5);
			}

			toSetup("PreScale") = v2f(1.2f * prescale, 1.2f * prescale);

			toSetup("Radius")=((v2f)toSetup("Size")).x*1.2*prescale*0.5f;

			toSetup["ChannelPercent"]("FontSize") = 0.6f* 24.0f / prescale;
			toSetup["ChannelPercent"]("MaxWidth") = 0.6f * 100.0f / prescale;
			toSetup["ChannelName"]("FontSize") = 0.6f * 18.0f / prescale;
			toSetup["ChannelName"]("MaxWidth") = 0.6f * 150.0f / prescale;

			const SP<UIImage>& checkTexture = toSetup;

			if (!checkTexture->HasTexture())
			{
				somethingChanged = true;
				if (toPlace.first->mThumb.mTexture)
				{
					checkTexture->addItem(toPlace.first->mThumb.mTexture);
				}
				else
				{
					YoutubeConnect::LoadChannelStruct(toPlace.second, *toPlace.first,true);
				}

			}
			if (toShowCount >= mMaxChannelCount) // really draw in screen
			{
				dock.Set(-5.0f, -5.0f);
				toSetup("Dock") = dock;

			}
		}
		toShowCount++;

		
	}

	if (mySubscribedWriters >= mSubscribedUserCount)
	{
		if( somethingChanged == false) 
		{
			if (mState != 50)
			{
				SaveStatFile();
				#ifdef LOG_ALL
				closeLog();
				#endif
				mState = 50; // finished
			}
			
			// clear unwanted texts when finished

			mMainInterface["ParsedComments"]("Text") = "";
			mMainInterface["RequestCount"]("Text") = "";
			mMainInterface["CurrentVideo"]("Text") = "";
			mMainInterface["switchForce"]("IsHidden") = false;
			mMainInterface["switchForce"]("IsTouchable") = true;

		}
	}*/
}

float	YoutubeAnalyser::PerChannelUserMap::GetNormalisedSimilitude(const PerChannelUserMap& other)
{
	/*unsigned int count = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (other.m[i] & m[i])
		{
			count++;
		}
	}

	float result = ((float)count) / ((float)(mSubscribedCount + other.mSubscribedCount - count));

	return result;*/
	return 0.0f;
}

float	YoutubeAnalyser::PerChannelUserMap::GetNormalisedAttraction(const PerChannelUserMap& other)
{
	/*unsigned int count = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (other.m[i] & m[i])
		{
			count++;
		}
	}

	float minSCount = (float)std::min(mSubscribedCount, other.mSubscribedCount);

	float result = ((float)count) / minSCount;

	return result;*/
	return 0.0f;
}

void	YoutubeAnalyser::prepareForceGraphData()
{

	/*Utils::Histogram<float>	hist({0.0,1.0}, 256);

	mChannelSubscriberMap.clear();

	// for each showed channel
	for (auto& c : mShowedChannels)
	{
		PerChannelUserMap	toAdd(mSubscriberList.size());
		int sindex = 0;
		int subcount = 0;
		for (auto& s : mSubscriberList)
		{
			if (mFoundUsers[s].isSubscriberOf(c.first))
			{
				toAdd.SetSubscriber(sindex);
			}
			++sindex;
		}
		toAdd.mThumbnail = c.second;
		c.second["ChannelPercent"]("Text") = "";
		c.second["ChannelName"]("Text") = "";
		mChannelSubscriberMap[c.first] = toAdd;
	}

	for (auto& l1 : mChannelSubscriberMap)
	{
		l1.second.mPos = l1.second.mThumbnail->getValue<v2f>("Dock");
		l1.second.mPos.x = 640+(rand()%129)-64;
		l1.second.mPos.y = 400+(rand()%81)-40;
		l1.second.mForce.Set(0.0f, 0.0f);
		l1.second.mRadius = l1.second.mThumbnail->getValue<float>("Radius");

		int index = 0;
		for (auto& l2 : mChannelSubscriberMap)
		{
			if (&l1 == &l2)
			{
				l1.second.mCoeffs.push_back({ index,-1.0f });
			}
			else
			{
				float coef = l1.second.GetNormalisedSimilitude(l2.second);
				l1.second.mCoeffs.push_back({ index ,coef });
				hist.addValue(coef);
			}
			++index;
		}

		// sort mCoeffs by value
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int,float>& a1, const std::pair<int, float>& a2)
			{
				if (a1.second == a2.second)
				{
					return (a1.first < a2.first);
				}

				return a1.second < a2.second;
			}
		);


		// sort again mCoeffs but by index 
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int, float>& a1, const std::pair<int, float>& a2)
			{
				return a1.first < a2.first;
			}
		);
	}

	hist.normalize();

	std::vector<float> histlimits = hist.getPercentList({0.05f,0.5f,0.96f});

	
	float rangecoef1 = 1.0f / (histlimits[1] - histlimits[0]);
	float rangecoef2 = 1.0f / (histlimits[2] - histlimits[1]);
	int index1 = 0;
	for (auto& l1 : mChannelSubscriberMap)
	{
		// recompute coeffs according to histogram
		int index2 = 0;
		for (auto& c : l1.second.mCoeffs)
		{
			if (index1 != index2)
			{
				// first half ?
				if ((c.second >= histlimits[0]) && (c.second <= histlimits[1]))
				{
					c.second -= histlimits[0];
					c.second *= rangecoef1;
					c.second -= 1.0f;
					c.second = c.second * c.second * c.second;
					c.second += 1.0f;
					c.second *= (histlimits[1] - histlimits[0]);
					c.second += histlimits[0];
				}
				else if ((c.second > histlimits[1]) && (c.second <= histlimits[2]))
				{
					c.second -= histlimits[1];
					c.second *= rangecoef2;
					c.second = c.second * c.second * c.second;
					c.second *= (histlimits[2] - histlimits[1]);
					c.second += histlimits[1];
				}
				c.second -= histlimits[1];
				c.second *= 2.0f;

			}
			++index2;
		}
		++index1;
	}*/
}

void	YoutubeAnalyser::DrawForceBased()
{
	/*v2f center(1280.0 / 2.0, 800.0 / 2.0);

	const float timeDelay = 10.0f;
	const float timeDivisor = 0.04f;

	float currentTime = ((GetApplicationTimer()->GetTime() - mForcedBaseStartingTime) - timeDelay) * timeDivisor;
	if (currentTime > 1.0f)
	{
		currentTime = 1.0f;
	}
	// first compute attraction force on each thumb
	for (auto& l1 : mChannelSubscriberMap)
	{
		PerChannelUserMap& current = l1.second;
	
		// always a  bit of attraction to center
		v2f	v(mThumbcenter);
		v -= current.mPos;

		// add a bit of random
		v.x += (rand() % 32) - 16;
		v.y += (rand() % 32) - 16;
		
		current.mForce *= 0.1f;
		current.mForce=v*0.001f;
	
		int i = 0;
		for (auto& l2 : mChannelSubscriberMap)
		{
			if (&l1 == &l2)
			{
				i++;
				continue;
			}
			
			v2f	v(l2.second.mPos - current.mPos);
			float dist = Norm(v);
			v.Normalize();
			float coef = current.mCoeffs[i].second;
			if (currentTime < 0.0f)
			{
				coef += -currentTime / (timeDelay* timeDivisor) ;
			}
			if (coef > 0.0f)
			{
				if (dist <= (current.mRadius + l2.second.mRadius)) // if not already touching
				{
					coef = 0.0f;
				}
			}
			
			current.mForce += v * coef * (l2.second.mRadius + current.mRadius) / ((l2.second.mRadius + current.mRadius) + (dist * dist) * 0.001);

			i++;
		}
	}
	// then move according to forces
	for (auto& l1 : mChannelSubscriberMap)
	{
		PerChannelUserMap& current = l1.second;
		current.mPos += current.mForce;
	}

	if (currentTime > 0.0f)
	{
		// then adjust position with contact
		for (auto& l1 : mChannelSubscriberMap)
		{
			PerChannelUserMap& current = l1.second;

			for (auto& l2 : mChannelSubscriberMap)
			{
				if (&l1 == &l2)
				{
					continue;
				}

				v2f	v(l2.second.mPos - current.mPos);
				float dist = Norm(v);
				v.Normalize();

				float r = (current.mRadius + l2.second.mRadius) * currentTime;
				if (dist < r)
				{
					float coef = (r - dist) * 0.5f;
					current.mPos -= v * coef;
					l2.second.mPos += v * coef;
				}
			}
		}
	}
	// then check if they don't get out of the screen
	for (auto& l1 : mChannelSubscriberMap)
	{

		PerChannelUserMap& current = l1.second;
		v2f	v(current.mPos);
		v -= center;
		float l1 = Norm(v);
		
		float angle = atan2f(v.y, v.x);
		v.Normalize();

		v2f ellipse(center);
		ellipse.x *= 0.9*cosf(angle);
		ellipse.y *= 0.9*sinf(angle);

		v2f tst = ellipse.Normalized();

		ellipse -= tst * current.mRadius;
		float l2 = Norm(ellipse);

		if (l1 > l2)
		{
			current.mPos -= v*(l1-l2);
		}

		v2f dock(current.mPos);
		dock.x /= 1280.0f;
		dock.y /= 800.0f;

		current.mThumbnail("Dock") = dock;
	}

	*/
}

void		YoutubeAnalyser::SaveStatFile()
{

	std::string filename = mChannelInfos.mName.ToString();
	filename += "_stats.csv";

	// avoid slash and backslash
	std::replace(filename.begin(), filename.end(), '/', ' ');
	std::replace(filename.begin(), filename.end(), '\\', ' ');

	filename = FilePathManager::MakeValidFileName(filename);

	// get all parsed channels and get the ones with more than mValidChannelPercent subscribed users
	std::vector<std::pair<YoutubeConnect::ChannelStruct*, std::string>>	toSave;
	for (auto c : mFoundChannels)
	{
		if (c.second->mSubscribersCount >= 1)
		{
			float percent = (float)c.second->mSubscribersCount / (float)mySubscribedWriters;
			if (percent > mValidChannelPercent)
			{
				toSave.push_back({ c.second,c.first });
			}
		}
	}

	std::sort(toSave.begin(), toSave.end(), [](const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a1, const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a2)
		{
			if (a1.first->mSubscribersCount == a2.first->mSubscribersCount)
			{
				return a1.second > a2.second;
			}
			return (a1.first->mSubscribersCount > a2.first->mSubscribersCount);
		}
	);

	std::vector<std::pair<YoutubeConnect::ChannelStruct*, std::string>>	toSaveUnsub;
	float unsubWriters = myPublicWriters -mySubscribedWriters;
	for (auto c : mFoundChannels)
	{
		if (c.second->mNotSubscribedSubscribersCount >= 1)
		{
			float percent = (float)c.second->mNotSubscribedSubscribersCount / unsubWriters;
			if (percent > mValidChannelPercent)
			{
				toSaveUnsub.push_back({ c.second,c.first });
			}
		}
	}

	std::sort(toSaveUnsub.begin(), toSaveUnsub.end(), [](const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a1, const std::pair<YoutubeConnect::ChannelStruct*, std::string>& a2)
		{
			if (a1.first->mNotSubscribedSubscribersCount == a2.first->mNotSubscribedSubscribersCount)
			{
				return a1.second > a2.second;
			}
			return (a1.first->mNotSubscribedSubscribersCount > a2.first->mNotSubscribedSubscribersCount);
		}
	);

	SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
	if (L_File->mFile)
	{
		// save general stats
		char saveBuffer[4096];
		sprintf(saveBuffer, "Total writers found,%d\n", mFoundUsers.size());
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "Public writers found,%d\n", myPublicWriters);
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "Subscribed writers found,%d\n", mySubscribedWriters);
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "Found channels Count,%d\n", mFoundChannels.size());
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());

		std::string header = "Channel Name,Channel ID,Percent,Jaccard\n";
		Platform_fwrite(header.c_str(), 1, header.length(), L_File.get());
		
		for (const auto& p : toSave)
		{
			int percent = (int)(100.0f * ((float)p.first->mSubscribersCount / (float)mySubscribedWriters));
			// A subscribers %
			float A_percent = ((float)p.first->mSubscribersCount / (float)mySubscribedWriters);
			// A intersection size with analysed channel
			float A_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A_percent;
			// A union size with analysed channel 
			float A_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)p.first->mTotalSubscribers - A_a_inter_b;

			int J = (int)(100.0f * A_a_inter_b / A_a_union_b);
			sprintf(saveBuffer, "\"%s\",%s,%d,%d\n",p.first->mName.ToString().c_str(),p.second.c_str(), percent,J);
			Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		}
		
		header = "\nNot subscribed writers\nChannel Name,Channel ID,Percent,J\n";
		Platform_fwrite(header.c_str(), 1, header.length(), L_File.get());

		for (const auto& p : toSaveUnsub)
		{
			int percent = (int)(100.0f * ((float)p.first->mNotSubscribedSubscribersCount / (float)unsubWriters));
			// A subscribers %
			float A_percent = ((float)p.first->mSubscribersCount / (float)mySubscribedWriters);
			// A intersection size with analysed channel
			float A_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A_percent;
			// A union size with analysed channel 
			float A_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)p.first->mTotalSubscribers - A_a_inter_b;

			int J = (int)(100.0f * A_a_inter_b / A_a_union_b);


			sprintf(saveBuffer, "\"%s\",%s,%d,%d\n", p.first->mName.ToString().c_str(), p.second.c_str(), percent,J);
			Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		}

		Platform_fclose(L_File.get());
	}
}

void	YoutubeAnalyser::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
	CoreDestroyModule(HTTPRequestModule);
}

void	YoutubeAnalyser::ProtectedInitSequence(const std::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;

		// launch fsm
		SP<CoreFSM> fsm = mFsm;
		fsm->setStartState("Init");
		fsm->Init();

		mGraphDrawer->setInterface(mMainInterface);
		mGraphDrawer->Init();

	}
}
void	YoutubeAnalyser::ProtectedCloseSequence(const std::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mGraphDrawer = nullptr;
		SP<CoreFSM> fsm = mFsm;
		mFsm = nullptr;
		removeItem(fsm);


		mMainInterface = nullptr;

		// delete all channel structs

		for (const auto& c : mFoundChannels)
		{
			delete c.second;
		}

		mFoundChannels.clear();
		mShowedChannels.clear();
		mChannelInfos.mThumb.mTexture = nullptr;
		mChannelSubscriberMap.clear();


#ifdef LOG_ALL
		closeLog();
#endif
	}
}


void	YoutubeAnalyser::switchDisplay()
{
	mMainInterface["switchForce"]("IsHidden") = true;
	mMainInterface["switchForce"]("IsTouchable") = false;

	mGraphDrawer->nextDrawType();
}

void	YoutubeAnalyser::switchForce()
{
	bool currentDrawForceState = mGraphDrawer->getValue<bool>("DrawForce");
	if (!currentDrawForceState)
	{
		mThumbcenter = (v2f)mMainInterface["thumbnail"]("Dock");
		mThumbcenter.x *= 1280.0f;
		mThumbcenter.y *= 800.0f;

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



void	YoutubeAnalyser::mainChannelID(const std::string& id)
{
	if (id != "")
	{
		mChannelID = id;
	}

	YoutubeConnect::SaveChannelID(mChannelName, mChannelID);

	KigsCore::Disconnect(mYoutubeConnect.get(), "ChannelIDRetrieved", this, "mainChannelID");

	requestDone(); // pop wait state
}

void	YoutubeAnalyser::getChannelStats(const YoutubeConnect::ChannelStruct& chan)
{
	if (chan.mID != "")
	{
		YoutubeConnect::SaveChannelStruct(chan);
	}

	KigsCore::Disconnect(mYoutubeConnect.get(), "ChannelStatsRetrieved", this, "getChannelStats");

	requestDone(); // pop wait state
}


void	YoutubeAnalyser::requestDone()
{
	mNeedWait = false;
}