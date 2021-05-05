#include "TwitterAccount.h"
#include "RTNetwork.h"
#include "CoreMap.h"

CoreItemSP TwitterAccount::loadTweetsFile()
{
	std::string filename = "Cache/UserNameRT/";
	filename += mUserStruct.mName.ToString() + "_" + mSettings->getFromDate() + "_" + mSettings->getToDate() +"_tweets.json";
	return RTNetwork::LoadJSon(filename, true,false);
}
void	TwitterAccount::saveTweetsFile(CoreItemSP toSave)
{
	std::string filename = "Cache/UserNameRT/";
	filename += mUserStruct.mName.ToString() + "_" + mSettings->getFromDate() + "_" + mSettings->getToDate() + "_tweets.json";

	RTNetwork::SaveJSon(filename, toSave, true);
}


bool	TwitterAccount::needMoreTweets()
{
	
	CoreItemSP tweets = loadTweetsFile();
	if (tweets.isNil())
	{
		return true;
	}
	else
	{
		CoreItemSP meta = tweets["meta"];
		if (!meta.isNil())
		{
			if (!meta["next_token"].isNil())
			{
				mNextTweetsToken = meta["next_token"];
				return true;
			}
		}
	}

	return false;
}

void		TwitterAccount::updateTweetList(CoreItemSP currentTwt)
{
	std::string txt = currentTwt["text"];

	u64 tweetid = currentTwt["id"];
	u32 like_count = currentTwt["public_metrics"]["like_count"];
	u32 rt_count = currentTwt["public_metrics"]["retweet_count"];
	u32 quote_count = currentTwt["public_metrics"]["quote_count"];

	bool needAdd = true;
	tweet toadd;
	toadd.mID = tweetid;
	toadd.mRTCount = rt_count;
	toadd.mQuoteCount = quote_count;
	toadd.mReferences.clear();

	bool searchHttps = true;

	CoreItemSP refs = currentTwt["referenced_tweets"];
	if (!refs.isNil())
	{
		needAdd = false;
		for (auto r : refs)
		{
			std::string type = r["type"];
			if ( (type == "quoted") || (type == "retweeted"))
			{
				toadd.mReferences.push_back( r["id"] );
				needAdd = true;
				searchHttps = false;	// search url references only if not quoted or retweet
			}
			else if(type == "replied_to")
			{
				if(toadd.mRTCount + toadd.mQuoteCount) // also include replies that where retweeted
					needAdd = true;
			}
		}
	}

	auto findURLEnd = [](const std::string& tosearch, size_t startpos)->size_t
	{
		size_t current = startpos;
		size_t l = tosearch.length();
		while (current < l)
		{
			bool goodOne=false;
			char currentC = tosearch[current];
			if ((currentC>='A') && (currentC <= 'Z'))
			{
				goodOne=true;
			}
			if ((currentC >= 'a') && (currentC <= 'z'))
			{
				goodOne = true;
			}
			if ((currentC >= '0') && (currentC <= '9'))
			{
				goodOne = true;
			}

			if (!goodOne)
				return current;

			++current;
		}
		return std::string::npos;
	};


	if (searchHttps)
	{
		if (currentTwt["attachments"].isNil()) // it's not an attachement (gif, image...)
		{
			size_t pos = txt.find("https://t.co/"); // a link was found

			while(pos != std::string::npos)
			{
				size_t endpos = findURLEnd(txt, pos + 13);
			
				size_t l;
				if (endpos != std::string::npos)
				{
					l = endpos - (pos + 13);
				}
				else
				{
					l = endpos;
				}

				toadd.mURLs.push_back(txt.substr(pos + 13, l));
				needAdd = true;
				pos = txt.find("https://t.co/", endpos);
			}
		}
	}

	if (toadd.mReferences.size() == 1) // when retweet, get user directly from txt when possible
	{
		if (txt.substr(0, 4) == "RT @")
		{
			size_t found = txt.find(':', 4);
			if (found != std::string::npos)
			{
				toadd.mRTedUser = txt.substr(4, found-4);
			}
		}
	}

	if(needAdd)
		mTweetList.push_back(toadd);

}

CoreItemSP	TwitterAccount::loadURL(const std::string& shortURL)
{
	std::string filename = "Cache/EncodedURL/" + shortURL.substr(0, 4) + "/" + shortURL + ".json";
	return RTNetwork::LoadJSon(filename, false, false);
}
void		TwitterAccount::saveURL(const std::string& shortURL, const std::string& fullURL)
{
	std::string filename = "Cache/EncodedURL/" + shortURL.substr(0, 4) + "/" + shortURL + ".json";

	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
	CoreItemSP decoded = CoreItemSP::getCoreItemOfType<CoreValue<std::string>>();
	decoded = fullURL;
	initP->set("URL", decoded);
	RTNetwork::SaveJSon(filename, initP, false);
}


CoreItemSP		TwitterAccount::loadYoutubeFile(const std::string& videoID)
{
	std::string filename = "Cache/YTVideoID/" + videoID.substr(0, 4) + "/" + videoID + ".json";
	return RTNetwork::LoadJSon(filename, false, false);
}
void			TwitterAccount::saveYoutubeFile(const std::string& videoID, const std::string& channelID)
{
	std::string filename = "Cache/YTVideoID/" + videoID.substr(0, 4) + "/" + videoID + ".json";

	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
	CoreItemSP channel = CoreItemSP::getCoreItemOfType<CoreValue<std::string>>();
	channel = channelID;
	initP->set("GUID", channel);
	RTNetwork::SaveJSon(filename, initP, false);
}


CoreItemSP	TwitterAccount::loadUserID(const std::string& uname)
{
	std::string filename = "Cache/Tweets/" + uname.substr(0, 4) + "/" + uname + ".json";
	return RTNetwork::LoadJSon(filename, false, false);
}
void		TwitterAccount::saveUserID(const std::string& uname, u64 id)
{
	std::string filename = "Cache/Tweets/" + uname.substr(0, 4) + "/" + uname + ".json";

	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
	CoreItemSP idP = CoreItemSP::getCoreItemOfType<CoreValue<u64>>();
	idP = id;
	initP->set("id", idP);
	RTNetwork::SaveJSon(filename, initP, false);
}

CoreItemSP	TwitterAccount::loadRetweeters(u64 twtid)
{
	std::string filename = "Cache/Tweet/";
	std::string folderName = RTNetwork::GetUserFolderFromID(twtid);
	filename += folderName + "/" + RTNetwork::GetIDString(twtid) + "_Retweeters.json";

	return RTNetwork::LoadJSon(filename, false,false);
}
void		TwitterAccount::saveRetweeters(u64 twtid, CoreItemSP tosave)
{
	std::string filename = "Cache/Tweet/";
	std::string folderName = RTNetwork::GetUserFolderFromID(twtid);
	filename += folderName + "/" + RTNetwork::GetIDString(twtid) + "_Retweeters.json";

	RTNetwork::SaveJSon(filename, tosave, false);
}

CoreItemSP	TwitterAccount::loadTweetUserFile(u64 twtid)
{
	std::string filename = "Cache/Tweet/";
	std::string folderName = RTNetwork::GetUserFolderFromID(twtid);
	filename += folderName + "/" + RTNetwork::GetIDString(twtid) + ".json";

	return RTNetwork::LoadJSon(filename, false,false);
}
void		TwitterAccount::saveTweetUserFile(u64 twtid, const std::string& username)
{
	
	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<std::string>>();
	initP->set("Name", CoreItemSP::getCoreValue(username));
	
	std::string folderName = RTNetwork::GetUserFolderFromID(twtid);

	std::string filename = "Cache/Tweet/";
	filename += folderName + "/" + RTNetwork::GetIDString(twtid) + ".json";

	RTNetwork::SaveJSon(filename, initP,false);
}

void		TwitterAccount::updateEmbeddedURLList()
{

	std::set<std::string>	alreadyAsked;


	for (const auto& t : mTweetList)
	{
		if ((t.mReferences.size()) && (t.mRTedUser.length() == 0)) // quoted reference ?
		{
			
		}
		else if (t.mURLs.size())
		{
			for (const auto& url : t.mURLs)
			{
				CoreItemSP urlfile = loadURL(url);

				if (urlfile.isNil()) // file is not available
				{
					if (alreadyAsked.find(url) == alreadyAsked.end())
					{
						mNeedURLs.push_back(url); // request url 
					}
					alreadyAsked.insert(url);
				}
			}
		}
	}
}

void		TwitterAccount::updateTweetRequestList()
{
	std::set<u64>	alreadyAsked;
	for (auto t : mTweetList)
	{
		if ((t.mReferences.size()) && (t.mRTedUser.length() == 0)) // quoted reference ?
		{
			for (auto u : t.mReferences)
			{
				CoreItemSP user = loadTweetUserFile(u); 

				if (user.isNil()) // and file is not available
				{
					if (alreadyAsked.find(u) == alreadyAsked.end())
					{
						mTweetToRequestList.push_back(u); // request user for this tweet
					}
					alreadyAsked.insert(u);
				}
			}
		}
	}
}

void		TwitterAccount::updateRetweeterList()
{
	mTweetToRequestList.clear();

	std::set<u64>	alreadyAsked;

	for (auto t : mTweetList)
	{
		if (t.mRTedUser.length() == 0) // tweet posted by current TwitterAccount or quoted 
		{
			if ((t.mRTCount + t.mQuoteCount)) // if this tweet was retweeted or quoted 
			{
				CoreItemSP users = loadRetweeters(t.mID); 

				if (users.isNil()) // and file is not available
				{
					if (alreadyAsked.find(t.mID) == alreadyAsked.end())
					{
						mTweetToRequestList.push_back(t.mID); // then request user ids list for this tweet 
					}
					alreadyAsked.insert(t.mID);
				}
			}
		}
	}
	
}

void		TwitterAccount::updateUserNameRequest()
{
	mUserNameRequestList.clear();
	mYoutubeVideoList.clear();
	for (const auto& t : mTweetList)
	{
		if (t.mRTedUser.length()) // user name found
		{
			CoreItemSP currentUserJson = loadUserID(t.mRTedUser);
			if (currentUserJson.isNil())
			{
				mUserNameRequestList.insert(t.mRTedUser);
			}
			else
			{
				mSettings->updateUserNameAndIDMaps(t.mRTedUser, currentUserJson["id"]);
			}
		}
		else if (t.mReferences.size())
		{
			for (auto u : t.mReferences) // quoted
			{
				CoreItemSP user = loadTweetUserFile(u);
				if (!user.isNil())
				{
					std::string uname = user["Name"];
					if (uname != "")
					{
						CoreItemSP currentUserJson = loadUserID(uname);
						if (currentUserJson.isNil())
						{
							mUserNameRequestList.insert(uname);
						}
						else
						{
							mSettings->updateUserNameAndIDMaps(uname, currentUserJson["id"]);
						}
					}
				}
			}
		}
		else if (t.mURLs.size())
		{
			for (auto& url : t.mURLs)
			{
				CoreItemSP urlfile = loadURL(url);
				if (!urlfile.isNil())
				{
					std::string wurl = urlfile["URL"];
					std::string uname = mSettings->getTwitterAccountForURL(wurl);
					if (uname != "")
					{
						if (uname.substr(0, 3) == "YT:")
						{
							mYoutubeVideoList.insert(uname.substr(3));
						}
						else
						{
							CoreItemSP currentUserJson = loadUserID(uname);
							if (currentUserJson.isNil())
							{
								mUserNameRequestList.insert(uname);
							}
							else
							{
								mSettings->updateUserNameAndIDMaps(uname, currentUserJson["id"]);
							}
						}
					}
				}
			}
		}
	}
}

void		TwitterAccount::addTweets(CoreItemSP json,bool addtofile)
{

	CoreItemSP fromfile = nullptr;
	if(addtofile)
		fromfile=loadTweetsFile();

	CoreItemSP tweetsArray = json["data"];
	if (!tweetsArray.isNil())
	{
		unsigned int tweetcount = tweetsArray->size();
		for (unsigned int i = 0; i < tweetcount; i++)
		{
			CoreItemSP currentTwt = tweetsArray[i];

			// add tweet to previous file
			if (!fromfile.isNil() && addtofile)
				fromfile["data"]->set("", currentTwt);

			if (!addtofile)
				updateTweetList(currentTwt); // update tweet list when all tweets were loaded
		}

	}


	if (!fromfile.isNil() && addtofile)
	{

		if (fromfile["includes"].isNil())
		{
			fromfile->set("includes", CoreItemSP::getCoreMap());
		}

		if (fromfile["includes"]["media"].isNil())
		{
			fromfile["includes"]->set("media", CoreItemSP::getCoreVector());
		}

		CoreItemSP addTo = fromfile["includes"]["media"];

		// add includes
		CoreItemSP includesMediaArray = json["includes"]["media"];
		if (!includesMediaArray.isNil())
		{
			unsigned int mediacount = includesMediaArray->size();
			for (unsigned int i = 0; i < mediacount; i++)
			{
				CoreItemSP currentMedia = includesMediaArray[i];

				addTo->set("", currentMedia);
			}
		}
	}

	std::string nextStr = "-1";

	CoreItemSP meta = json["meta"];
	if (!meta.isNil())
	{
		if (!meta["next_token"].isNil())
		{
			nextStr = meta["next_token"];
			if (nextStr == "0")
			{
				nextStr = "-1";
			}
		}
	}

	if (!fromfile.isNil() && addtofile)
	{
		fromfile->set("meta",meta);
	}
	else
	{
		fromfile = json;
	}
	
	if(addtofile)
		saveTweetsFile(fromfile);

	mNextTweetsToken = nextStr;
}


void		TwitterAccount::buildNetworkList()
{
	mSortedNetwork.clear();

	std::unordered_map<u64, std::pair<u32,u32>>	coefs;

	for (auto n : mCountHasRT)
	{
		if (coefs.find(n.first) == coefs.end())
		{
			coefs.insert({ n.first,{n.second,0} });
		}
		else
		{
			coefs[n.first].first += n.second;
		}
	}

	for (auto n : mCountWasRT)
	{
		if (coefs.find(n.first) == coefs.end())
		{
			coefs.insert({ n.first,{0,n.second} });
		}
		else
		{
			coefs[n.first].second += n.second;
		}
	}

	for (auto n : coefs)
	{
		mSortedNetwork.push_back(n);
	}


	std::sort(mSortedNetwork.begin(), mSortedNetwork.end(), [](const std::pair<u64, std::pair<u32, u32>>& a1, const std::pair<u64,std::pair<u32, u32>>& a2)
		{
			u32 sum1 = a1.second.first + a1.second.second;
			u32 sum2 = a2.second.first + a2.second.second;

			if (sum1 == sum2)
			{
				return a1.first > a2.first;
			}
			return (sum1 > sum2);
		}
	);
}


void		TwitterAccount::updateStats()
{

	mCountHasRT.clear(); // users retweeted by this account
	mCountWasRT.clear(); // users who retweeted a tweet from this account
	mNotRetweetCount = 0;
	mHasRetweetCount = 0;
	mWasRetweetCount = 0;

	std::map<u64, std::string>& lUserIDToNameMap = mSettings->getUserIDToNameMap();
	std::map<std::string, u64>&	lNameToUserIDMap = mSettings->getNameToUserIDMap();
	
	auto registerWASRT = [&](const tweet& t) {

		if (t.mRTCount + t.mQuoteCount) // if this tweet was retweeted or quoted 
		{
			bool wasReallyRT = false;
			bool wasRTByMe = false;
			CoreItemSP users = loadRetweeters(t.mID);
			CoreItemSP ids = users["ids"];
			for (auto i : ids)
			{
				u64 id = i;
				if (id && (id != mID))  // don't count myself
				{
					if (mCountWasRT.find(id) == mCountWasRT.end())
					{
						mCountWasRT[id] = 1;
					}
					else
					{
						mCountWasRT[id]++;
					}
					wasReallyRT = true;
				}
				else if (id == mID)
				{
					wasRTByMe = true;
				}
			}
			if (wasReallyRT)
				mWasRetweetCount++;
			else if (wasRTByMe)
				mOwnRetweetCount++;
		}
		else
		{
			mNotRetweetCount++;
		}
	};


	for (const auto & t : mTweetList)
	{
		if (t.mRTedUser.length()) // retweet by this
		{
			if (lNameToUserIDMap.find(t.mRTedUser) == lNameToUserIDMap.end())
			{
				KIGS_ERROR("should not be here", 2);
			}
			else
			{
				u64 id = lNameToUserIDMap[t.mRTedUser];
				if (id && (id != mID)) // don't count myself
				{
					if (mCountHasRT.find(id) == mCountHasRT.end())
					{
						mCountHasRT[id] = 1;
					}
					else
					{
						mCountHasRT[id]++;
					}
					mHasRetweetCount++;
				}
			}
		}
		else if(t.mReferences.size() == 0)
		{
			// same as quoted
			
			if (t.mURLs.size())
			{
				for (auto url : t.mURLs)
				{
					CoreItemSP urlfile = loadURL(url);
					if (!urlfile.isNil())
					{
						std::string wurl = urlfile["URL"];
						std::string uname = mSettings->getTwitterAccountForURL(wurl);
						if (uname != "")
						{
							if (uname.substr(0, 3) == "YT:")
							{
								CoreItemSP YTfile = loadYoutubeFile(uname.substr(3));

								if (YTfile.isNil())
								{
									KIGS_ERROR("should not be here", 2);
								}
								else
								{
									std::string ChannelGUID = YTfile["GUID"];
									uname = mSettings->getWebToTwitterUser(ChannelGUID);
								}

							}

							if(uname!="")
							{
								if (lNameToUserIDMap.find(uname) == lNameToUserIDMap.end())
								{
									KIGS_ERROR("should not be here", 2);
								}
								else
								{

									u64 id = lNameToUserIDMap[uname];
									if (id && (id != mID))
									{
										if (mCountHasRT.find(id) == mCountHasRT.end())
										{
											mCountHasRT[id] = 1;
										}
										else
										{
											mCountHasRT[id]++;
										}
										mHasRetweetCount++;
									}
									else if (id == mID)
									{
										mOwnRetweetCount++;
									}
								}
							}
						}
					}
				}
			}

			registerWASRT(t);

		}
		else  // quoted
		{
			for (auto u : t.mReferences)
			{
				CoreItemSP user = loadTweetUserFile(u);

				if (user.isNil() || user["Name"].isNil()) // and file is not available
				{
					KIGS_ERROR("should not be here", 2);
				}
				else
				{
					std::string uname = user["Name"];
					if (uname != "")
					{
						if (lNameToUserIDMap.find(uname) == lNameToUserIDMap.end())
						{
							KIGS_ERROR("should not be here", 2);
						}
						else
						{
							u64 id = lNameToUserIDMap[uname];
							if (id && (id != mID))
							{
								if (mCountHasRT.find(id) == mCountHasRT.end())
								{
									mCountHasRT[id] = 1;
								}
								else
								{
									mCountHasRT[id]++;
								}
								mHasRetweetCount++;
							}
							else if (id == mID)
							{
								mOwnRetweetCount++;
							}
						}
					}
				}
			}
			registerWASRT(t);
		}
	}

	mConnectionCount = mHasRetweetCount + mWasRetweetCount;
	mSourceCoef = (float)mWasRetweetCount / (float)(mTweetList.size()- mOwnRetweetCount);

	buildNetworkList();
}

u32 TwitterAccount::getLinkCoef(TwitterAccount* other)
{
	for (auto c : mSortedNetwork)
	{
		if (c.first == other->mID)
		{
			return c.second.first + c.second.second;
		}
	}
	return 0;
}

u32 TwitterAccount::getLinkCoef(u64 otherID)
{
	for (auto c : mSortedNetwork)
	{
		if (c.first == otherID)
		{
			return c.second.first + c.second.second;
		}
	}
	return 0;
}

u32 TwitterAccount::getHasRTCoef(u64 otherID)
{
	for (auto c : mSortedNetwork)
	{
		if (c.first == otherID)
		{
			return c.second.first;
		}
	}
	return 0;
}

u32 TwitterAccount::getWasRTCoef(u64 otherID)
{
	for (auto c : mSortedNetwork)
	{
		if (c.first == otherID)
		{
			return c.second.second;
		}
	}
	return 0;
}