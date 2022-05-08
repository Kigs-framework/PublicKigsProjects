#include "TLTightness.h"
#include "FilePathManager.h"
#include "NotificationCenter.h"
#include "CoreFSM.h"
#include "JSonFileParser.h"
#include "HTTPRequestModule.h"
#include "TextureFileManager.h"

IMPLEMENT_CLASS_INFO(TLTightness);

IMPLEMENT_CONSTRUCTOR(TLTightness)
{

}

void	TLTightness::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<FilePathManager> pathManager = KigsCore::Singleton<FilePathManager>();
	pathManager->AddToPath(".", "xml");
	// when a path is given, search the file only with this path
	pathManager->setValue("StrictPath", true);
	pathManager->AddToPath(".", "json");

	CoreCreateModule(HTTPRequestModule, 0);


	// Init App
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary("launchParams.json");

	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), TwitterConnect, TwitterConnect, TwitterConnect);

	mTwitterConnect = KigsCore::GetInstanceOf("mTwitterConnect", "TwitterConnect");

	mTwitterConnect->initBearer(initP);


	auto SetMemberFromParam = [&](auto& x, const char* id) {
		if (initP[id]) x = initP[id].value<std::remove_reference<decltype(x)>::type>();
	};

	SetMemberFromParam(mUserName, "UserName");

	int oldFileLimitInDays = 3 * 30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");

	mTwitterConnect->initConnection(60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays);

	KigsCore::Connect(mTwitterConnect.get(),"done", this, "requestDone");

	TwitterConnect::UserStruct	mainuser;
	mainuser.mID = 0;
	mRetreivedUsers.push_back(mainuser);

	// setup FSM
	initCoreFSM();

	// add FSM
	SP<CoreFSM> fsm = KigsCore::GetInstanceOf("fsm", "CoreFSM");
	addItem(fsm);
	// Appear state
	fsm->addState("InitTL", new CoreFSMStateClass(TLTightness, InitTL)());
	SP<CoreFSMTransition> waittransition = KigsCore::GetInstanceOf("waittransition", "CoreFSMOnValueTransition");
	waittransition->setValue("TransitionBehavior", "Push");
	waittransition->setValue("ValueName", "NeedWait");
	waittransition->setState("Wait");
	waittransition->Init();

	SP<CoreFSMTransition> getfollowingtransition = KigsCore::GetInstanceOf("getfollowingtransition", "CoreFSMInternalSetTransition");
	getfollowingtransition->setState("GetFollowing");
	getfollowingtransition->Init();

	fsm->getState("InitTL")->addTransition(waittransition);
	fsm->getState("InitTL")->addTransition(getfollowingtransition);

	fsm->addState("Wait", new CoreFSMStateClass(TLTightness, Wait)());
	SP<CoreFSMTransition> waitendtransition = KigsCore::GetInstanceOf("waitendtransition", "CoreFSMOnValueTransition");
	waitendtransition->setValue("TransitionBehavior", "Pop");
	waitendtransition->setValue("ValueName", "NeedWait");
	waitendtransition->setValue("NotValue", true); // end wait when NeedWait is false
	waitendtransition->Init();

	fsm->getState("Wait")->addTransition(waitendtransition);

	fsm->addState("GetFollowing", new CoreFSMStateClass(TLTightness, GetFollowing)());

	fsm->getState("GetFollowing")->addTransition(waittransition);

	SP<CoreFSMTransition> treatusertransition = KigsCore::GetInstanceOf("treatusertransition", "CoreFSMInternalSetTransition");
	treatusertransition->setState("TreatUser");
	treatusertransition->Init();

	fsm->getState("GetFollowing")->addTransition(treatusertransition);

	fsm->addState("TreatUser", new CoreFSMStateClass(TLTightness, TreatUser)());

	SP<CoreFSMTransition> getuserdetailtransition = KigsCore::GetInstanceOf("getuserdetailtransition", "CoreFSMInternalSetTransition");
	getuserdetailtransition->setState("GetUserDetail");
	getuserdetailtransition->Init();

	SP<CoreFSMTransition> donetransition = KigsCore::GetInstanceOf("donetransition", "CoreFSMInternalSetTransition");
	donetransition->setState("Done");
	donetransition->Init();

	fsm->getState("TreatUser")->addTransition(getuserdetailtransition);
	fsm->getState("TreatUser")->addTransition(donetransition);

	fsm->addState("GetUserDetail", new CoreFSMStateClass(TLTightness, GetUserDetail)());

	fsm->getState("GetUserDetail")->addTransition(getfollowingtransition);
	
	fsm->getState("GetUserDetail")->addTransition(waittransition);


	fsm->addState("Done", new CoreFSMStateClass(TLTightness, Done)());



	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
}


void	TLTightness::ProcessingGraphicUpdate()
{
	if (mMainInterface)
	{
		if (GetApplicationTimer()->GetDelay(this) > 1.0)
		{
			GetApplicationTimer()->ResetDelay(this);

			u32 treatedUserCount = mRetreivedUsers.size();
			u32 totalUserCount = 1;
			if (mRetreivedUsers[0].mFollowing.size())
			{
				totalUserCount = mRetreivedUsers[0].mFollowing.size() + 1;
			}

			char textBuffer[256];
			sprintf(textBuffer, "Treated Users : %d / %d", treatedUserCount, totalUserCount);

			mMainInterface["TreatedAccount"]("Text") = textBuffer;

			sprintf(textBuffer, "Request Count : %d", mTwitterConnect->getRequestCount());

			mMainInterface["RequestCount"]("Text") = textBuffer;

			sprintf(textBuffer, "Request Delay : %f", mTwitterConnect->getDelay());

			mMainInterface["RequestWait"]("Text") = textBuffer;

		}
	}
}



void	TLTightness::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();

	
	
}

void	TLTightness::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
}

void	TLTightness::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		// launch fsm
		SP<CoreFSM> fsm = GetFirstSonByName("CoreFSM", "fsm");
		fsm->setStartState("InitTL");
		fsm->Init();
	}
}
void	TLTightness::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = nullptr;
		mRetreivedUsers.clear();
	}
}


void	TLTightness::requestDone()
{
	mNeedWait = false;
}

void TLTightness::mainUserDone()
{
	// save user
	JSonFileParser L_JsonParser;
	CoreItemSP initP = MakeCoreMap();
	initP->set("id", mRetreivedUsers[0].mID);
	std::string filename = "Cache/UserName/";
	filename += mUserName + ".json";
	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);

	KigsCore::Disconnect(mTwitterConnect.get(), "MainUserDone", this, "mainUserDone");

	requestDone();
}
// FSM

void CoreFSMStartMethod(TLTightness, InitTL)
{
	
}

void CoreFSMStopMethod(TLTightness, InitTL)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TLTightness, InitTL))
{
	ProcessingGraphicUpdate();
	std::string currentUserProgress = "Cache/"; 
	currentUserProgress += "UserName/";
	currentUserProgress += mUserName + ".json";
	CoreItemSP currentP = mTwitterConnect->LoadJSon(currentUserProgress);

	if (!currentP) // new user
	{
		KigsCore::Connect(mTwitterConnect.get(),"MainUserDone", this, "mainUserDone");
		mTwitterConnect->launchUserDetailRequest(mUserName, mRetreivedUsers[0],"MainUserDone");
		mNeedWait = true;
	}
	else // load current user
	{
		u64 userID = currentP["id"];
		mTwitterConnect->LoadUserStruct(userID, mRetreivedUsers[0], true);
		GetUpgrador()->activateTransition("getfollowingtransition");
	}
	return false;
}






void	CoreFSMStartMethod(TLTightness, Wait)
{
	
}
void	CoreFSMStopMethod(TLTightness, Wait)
{
	
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TLTightness, Wait))
{
	ProcessingGraphicUpdate();
	return false;
}


void	CoreFSMStartMethod(TLTightness, GetFollowing)
{

}
void	CoreFSMStopMethod(TLTightness, GetFollowing)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TLTightness, GetFollowing))
{
	ProcessingGraphicUpdate();
	u64 id = mRetreivedUsers[mCurrentUserIndex].mID;
	std::string filename = "Cache/Users/" + TwitterConnect::GetUserFolderFromID(id) + "/" + TwitterConnect::GetIDString(id) + ".ids";

	bool fileExist;
	auto followinglist=TwitterConnect::LoadIDVectorFile(filename, fileExist);

	if (fileExist)
	{
		mRetreivedUsers[mCurrentUserIndex].mFollowing = followinglist;
		GetUpgrador()->activateTransition("treatusertransition");
	}
	else
	{
		mTwitterConnect->launchGetFollowing(mRetreivedUsers[mCurrentUserIndex]);
		mNeedWait = true;
	}
	return false;
}

void	CoreFSMStartMethod(TLTightness, TreatUser)
{
	mCurrentUserIndex++;
}
void	CoreFSMStopMethod(TLTightness, TreatUser)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TLTightness, TreatUser))
{
	ProcessingGraphicUpdate();
	if (mRetreivedUsers.size() <= mRetreivedUsers[0].mFollowing.size())
	{
		mRetreivedUsers.emplace_back();
		GetUpgrador()->activateTransition("getuserdetailtransition");
	}
	else // ok everything is done
	{
		GetUpgrador()->activateTransition("donetransition");
	}
	return false;

}

void	CoreFSMStartMethod(TLTightness, GetUserDetail)
{
	
}
void	CoreFSMStopMethod(TLTightness, GetUserDetail)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TLTightness, GetUserDetail))
{
	ProcessingGraphicUpdate();
	u64 userID = mRetreivedUsers[0].mFollowing[mCurrentUserIndex-1];

	if (mTwitterConnect->LoadUserStruct(userID, mRetreivedUsers[mCurrentUserIndex], true))
	{
		GetUpgrador()->activateTransition("getfollowingtransition");
	}
	else
	{
		mTwitterConnect->launchUserDetailRequest(userID, mRetreivedUsers[mCurrentUserIndex]);
		mNeedWait = true;
	}
	return false;
}

void	CoreFSMStartMethod(TLTightness, Done)
{
	std::map<u64, u32>	occurences;

	auto insertInOccurence = [&](u64 id) 
	{
		if (occurences.find(id) == occurences.end())
		{
			occurences[id] = 1;
		}
		else
		{
			occurences[id]++;
		}
	};

	for (const auto& u : mRetreivedUsers)
	{
		insertInOccurence(u.mID);
		for (u64 id : u.mFollowing)
		{
			insertInOccurence(id);
		}
	}

	std::map<u64, float>	scores;
	for (const auto& u : mRetreivedUsers)
	{
		scores[u.mID] = 0.0f;
		for (u64 id : u.mFollowing)
		{
			if (occurences[id] == 1)
			{
				scores[u.mID]++;
			}
		}
	}

	// log 
	float meanScore = 0.0f;
	for (auto& sc : scores)
	{
		if (sc.first == mRetreivedUsers[0].mID)
			continue;

		if(sc.second)
			sc.second = logf(sc.second);

		meanScore += sc.second;
	}

	meanScore /= (float)(scores.size() - 1);

	GetUpgrador()->mTotalReachableAccount = occurences.size()-1; // remove treated account
	GetUpgrador()->mMeanScore = meanScore;

	u32 countUnderScore = 0;
	for (auto& sc : scores)
	{
		if (sc.first == mRetreivedUsers[0].mID)
			continue;

		if (sc.second < meanScore)
			countUnderScore++;
	}

	// sort following
	std::vector<u32>	sortedIndex;
	for (u32 i = 1; i < mRetreivedUsers.size(); i++)
	{
		sortedIndex.push_back(i);
	}

	std::sort(sortedIndex.begin(), sortedIndex.end(), [&](u32 a, u32 b)->bool {
		
		return scores[mRetreivedUsers[a].mID] < scores[mRetreivedUsers[b].mID];
		
		});

	std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(mRetreivedUsers[0].mID);
	CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
	toAdd->addItem(mRetreivedUsers[0].mThumb.mTexture);
	toAdd("Dock")= v2f(0.15,0.15);
	toAdd("PreScale")=v2f(2.0, 2.0);
	toAdd["ChannelName"]("Text")= mRetreivedUsers[0].mName;
	toAdd["ChannelName"]("PreScale") = v2f(0.5, 0.5);
	mMainInterface->addItem(toAdd);


	// add each thumb in bubble spiral
	float	spiralinnerradius = 0.0f;
	float	angle = 0.0f;
	v2f		centerpos(1920.0f/4.0f,200.0f+880.0f/2.0);

	// position / bubble radius / spiral inner radius
	std::vector<std::pair<v2f, v2f>>	positions;

	// 2 row 2 column
	float rotationmatrix[2][2] = {  {cosf(-PI / 90.0f),-sinf(-PI / 90.0f)},
									{sinf(-PI / 90.0f),cosf(-PI / 90.0f)} };

	std::vector<CMSP> allbubbles;

	for(u32 i=0;i< countUnderScore;i++)
	{
		const auto& u = mRetreivedUsers[sortedIndex[i]];
		std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(u.mID);
		CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
		toAdd->addItem(u.mThumb.mTexture);

		float prescale = 0.4f + 0.6f * (scores[u.mID] / meanScore);
		float radius = 36.0f * prescale;
		
		v2f pos;
		
		if (i==0)
		{
			spiralinnerradius = radius;
			pos.Set(centerpos.x + (spiralinnerradius+ radius) * cosf(angle), centerpos.y + (spiralinnerradius + radius) * sinf(angle));
		}
		else
		{
			spiralinnerradius = positions.back().second.y;
			float prevradius = positions.back().second.x;

			float a = spiralinnerradius+ prevradius;
			float b = prevradius + radius;
			float c = spiralinnerradius + radius;

			float b2 = b * b;

			float nextangle = acosf((a*a+c*c-b2)/(2*a*c));
			angle += nextangle;

			v2f dir(cosf(angle), sinf(angle));
			pos.Set(centerpos.x + c * cosf(angle), centerpos.y + c * sinf(angle));

			bool wasmoved = false;
			// check if we need to move bubble
			for (u32 p = 0; p < positions.size()-1; p++)
			{
				const auto& prev = positions[p];
				bool needmove = true;
				while (needmove)
				{
					needmove = false;
					v2f delta(pos - prev.first);
					float distsqr = NormSquare(delta);

					if (distsqr < b2)
					{
						v2f lastdelta(pos - positions.back().first);
						v2f newdelta;
						newdelta.x = lastdelta.x * rotationmatrix[0][0] + lastdelta.y * rotationmatrix[0][1];
						newdelta.y = lastdelta.x * rotationmatrix[1][0] + lastdelta.y * rotationmatrix[1][1];
						pos = positions.back().first + newdelta;
						needmove = true;
						wasmoved = true;
					}
				}
			}

			// recompute angle
			if (wasmoved)
			{
				v2f lastdelta(pos - centerpos);
				float d=Norm(lastdelta);
				lastdelta /= d;

				angle = atan2f(lastdelta.y, lastdelta.x);

				spiralinnerradius = d - radius;
			}


		}

		positions.push_back({ pos,{radius,spiralinnerradius} });

		toAdd("PreScale") = v2f(prescale, prescale);
		toAdd->removeItem(toAdd["ChannelName"]);
		mMainInterface->addItem(toAdd);
		allbubbles.push_back(toAdd);
	}

	// adjust angle

	float wantedAngle = -PI / 2.0f;
	float deltaAngle = wantedAngle - angle;

	rotationmatrix[0][0] = cosf(deltaAngle);
	rotationmatrix[0][1] = -sinf(deltaAngle);
	rotationmatrix[1][0] = sinf(deltaAngle);
	rotationmatrix[1][1] = cosf(deltaAngle);

	for (u32 i = 0; i < countUnderScore; i++)
	{
		v2f pos = positions[i].first;
		v2f lastdelta(pos - centerpos);
		v2f newdelta;
		newdelta.x = lastdelta.x * rotationmatrix[0][0] + lastdelta.y * rotationmatrix[0][1];
		newdelta.y = lastdelta.x * rotationmatrix[1][0] + lastdelta.y * rotationmatrix[1][1];
		pos = centerpos + newdelta;

		pos.x *= 1.0f / 1920.0f;
		pos.y *= 1.0f / 1080.0f;
		allbubbles[i]("Dock") = pos;
	}

	float maxscore = scores[mRetreivedUsers[sortedIndex.back()].mID]- meanScore;

	float current_t = 0.0f;
	float current_x = 0.0f;
	for (u32 i = countUnderScore; i < sortedIndex.size(); i++)
	{
		const auto& u = mRetreivedUsers[sortedIndex[i]];
		std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(u.mID);
		CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
		toAdd->addItem(u.mThumb.mTexture);

		float prescale = 1.0f;
		float radius = 36.0f * prescale;
		float sqr2radius = 4.0f*radius * radius;

		float x = (scores[u.mID] - meanScore) / maxscore;
		if (x < current_x)
			x = current_x;

		float sine_t = 0.005f;

		v2f pos;
		
		bool needmove = true;
		do
		{
			pos.x = 1820.0f / 2.0f + (1720.0f / 2.0f) * x;
			pos.y = 1080.0f / 2.0f + (880.0f / 2.0f) * sinf((sine_t+ current_x) * PI * 10.0f);

			needmove = false;
			for (u32 p = countUnderScore; p < positions.size(); p++)
			{
				const auto& prev = positions[p];

				v2f delta(pos - prev.first);
				float distsqr = NormSquare(delta);

				if (distsqr < sqr2radius)
				{
					sine_t += 0.005f;
					needmove = true;
					if (sine_t > 0.2f)
					{
						x += 0.005f;
						sine_t = 0.0f;
					}
					break;
				}
			}
		} while (needmove);

		current_t += sine_t;
		current_x = x;

		positions.push_back({ pos,{radius,0.0f} });
		pos.x *= 1.0f / 1920.0f;
		pos.y *= 1.0f / 1080.0f;
		toAdd("Dock") = pos;
		toAdd("PreScale") = v2f(prescale, prescale);
		toAdd->removeItem(toAdd["ChannelName"]);
		mMainInterface->addItem(toAdd);
	}

}
void	CoreFSMStopMethod(TLTightness, Done)
{

}

DEFINE_UPGRADOR_METHOD(CoreFSMStateClass(TLTightness, Done), DoneGraphicUpdate)
{
	
	if (mMainInterface)
	{
		if (GetApplicationTimer()->GetDelay(this) > 1.0)
		{
			GetApplicationTimer()->ResetDelay(this);
			u32 totalAccount = GetUpgrador()->mTotalReachableAccount;

			char textBuffer[256];
			sprintf(textBuffer, "%d following => %d account",(u32) mRetreivedUsers[0].mFollowing.size(), totalAccount);

			mMainInterface["TreatedAccount"]("Text") = textBuffer;

			sprintf(textBuffer, "Mean Score : %f", GetUpgrador()->mMeanScore);

			mMainInterface["RequestCount"]("Text") = textBuffer;

			mMainInterface["RequestWait"]("Text") = "";

		}
	}
	return false;
}


DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(TLTightness, Done))
{
	std::vector<CoreModifiableAttribute*> noparams;
	DoneGraphicUpdate(this, noparams,nullptr);
	return false;
}

