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
	fsm->getState("RetrieveVideo")->addTransition(mTransitionList["userlistdetailtransition"]);

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
	fsm->getState("ProcessVideo")->addTransition(mTransitionList["userlistdetailtransition"]);


	// create GetAuthorInfos transition (Push)
	SP<CoreFSMTransition> getauthorinfostransition = KigsCore::GetInstanceOf("getauthorinfostransition", "CoreFSMInternalSetTransition");
	getauthorinfostransition->setValue("TransitionBehavior", "Push");
	getauthorinfostransition->setState("GetAuthorInfos");
	getauthorinfostransition->Init();

	// create TreatAuthors state
	fsm->addState("TreatAuthors", new CoreFSMStateClass(YoutubeAnalyser, TreatAuthors)());
	fsm->getState("TreatAuthors")->addTransition(getauthorinfostransition);
	fsm->getState("TreatAuthors")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("TreatAuthors")->addTransition(mTransitionList["userlistdetailtransition"]);


	// create RetrieveSubscriptions transition (Push)
	SP<CoreFSMTransition> retrievesubscriptiontransition = KigsCore::GetInstanceOf("retrievesubscriptiontransition", "CoreFSMInternalSetTransition");
	retrievesubscriptiontransition->setValue("TransitionBehavior", "Push");
	retrievesubscriptiontransition->setState("RetrieveSubscriptions");
	retrievesubscriptiontransition->Init();

	// create GetAuthorInfos state
	fsm->addState("GetAuthorInfos", new CoreFSMStateClass(YoutubeAnalyser, GetAuthorInfos)());
	fsm->getState("GetAuthorInfos")->addTransition(retrievesubscriptiontransition);
	fsm->getState("GetAuthorInfos")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("GetAuthorInfos")->addTransition(mTransitionList["userlistdetailtransition"]);


	// create RetrieveSubscriptions state
	fsm->addState("RetrieveSubscriptions", new CoreFSMStateClass(YoutubeAnalyser, RetrieveSubscriptions)());
	fsm->getState("RetrieveSubscriptions")->addTransition(mTransitionList["waittransition"]);
	fsm->getState("GetAuthorInfos")->addTransition(mTransitionList["userlistdetailtransition"]);


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
		mChannelInfos.mThumb.mTexture = nullptr;


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

bool	YoutubeAnalyser::checkDone()
{
	return (mSubscribedAuthorInfos.size() >= mSubscribedUserCount);
}