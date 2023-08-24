#include "flickrd2lib.h"
#include "browserhacker.h"
#include "utils/datetime.h"
#include "regexp/regexp.h"

namespace Fd2 {
#include "utils/SimpleJSON/json.hpp"
};

#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#define STATE(x) StateGuard s(this, x); if (!s.Valid()) return;

static std::string ParseFor(const std::string& str, const std::string& exp)
{
    std::string ret;
    LF::re::RegExp reg(exp);
    int r = reg.FindAll(str);
    //SDEB("FOUND: %d", r);
    //for (int i = 0; i < r; ++i)
    //{
    //    SINFO("%d -> %s", i, reg.Cap(i).c_str());
    //}
    if (r > 1)
    {
        ret = reg.Cap(1);
    }
    return ret;
}

class GetData
{
public:
    std::shared_ptr<LF::www::UrlGet> mGet;
    std::string mRequest;
};

void LF::Flickrd2::LoadCookies(Browser_t browser)
{
    STATE(InternalState_t::LoadCookies);

    BrowserHacker::Browser_t br = BrowserHacker::Browser_t::Chrome;
    switch (browser)
    {
    case Browser_t::Firefox:
        br = BrowserHacker::Browser_t::Firefox;
        break;
    case Browser_t::Opera:
        br = BrowserHacker::Browser_t::Opera;
        break;
    case Browser_t::Vivaldi:
        br = BrowserHacker::Browser_t::Vivaldi;
        break;
    }

    BrowserHacker hacker(br);
    if (hacker.BrowserValid())
    {
        mCookies = hacker.GetCookies("%%flickr.com");

        std::string cookie_accid;
        std::string cookie_epass;
        std::string cookieSession = "118205923%3A31d9e76722e34be9e4ef8aaaf78faf51";

        for (auto& c : mCookies->Get())
        {
            if (c.second->GetName() == "cookie_accid")
            {
                cookie_accid = c.second->GetValue();
            }
            else if (c.second->GetName() == "cookie_epass")
            {
                cookie_epass = c.second->GetValue();
            }
            if (!cookie_epass.empty() && !cookie_accid.empty())
            {
                break;
            }
        }

        if (!cookie_epass.empty() && !cookie_accid.empty())
        {
            cookieSession = cookie_accid + "%3A" + cookie_epass;
            //SUCC("cookieSession: %s", cookieSession.c_str());
            mCookies->Add("cookie_session", cookieSession);
        }

        SUCC("Loaded cookies: %d cookie session: %s", mCookies->Get().size(), cookieSession.empty() ? "NOT FOUND" : "OK");
    }
    else
    {
        SWARN("Failed to load cookies");
    }
}

void LF::Flickrd2::OnRequestComplete(void* userData, LF::www::UrlGet::RequestResult_t res)
{
    GetData* data = reinterpret_cast<GetData*>(userData);

    SDEB("Complete: %d\n\t%s", res, data->mRequest.c_str());

    switch (mState)
    {
    case InternalState_t::IDLE:
    case InternalState_t::LoadCookies:
        SWARN("No Request expected in this state");
        break;
    case InternalState_t::LoadApiKey:
        OnRequestComplete_LoadApiKey(data->mGet->GetResponse().mCurrentResponse);
        break;
    default:
        PRINT("===== RESPONSE ====\n%s\===================\n", data->mGet->GetResponse().mCurrentResponse.c_str());
    }
    delete data;
}

void LF::Flickrd2::OnRequestComplete_LoadApiKey(const std::string& resp)
{
#if 0
    LF::utils::DateTime now;
    now.Now();
    LF::fs::File f(std::string("C:/temp/fd2/") + now.ToString("file"));
    if (f.CreateOpen())
    {
        f.Put((uint8_t*)resp.data(), resp.size());
    }
#endif
    mRootAuth.mAppKey = ParseFor(resp, "root\\.YUI_config\\.flickr\\.api\\.site_key = \"(.*)\";");
    std::string ret = ParseFor(resp, "root\\.auth = (.*);");
    if (!ret.empty())
    {
        Fd2::json::JSON j = Fd2::json::JSON::Load(ret);
        mRootAuth.mSignedIn = j.at("signedIn").ToBool();
        mRootAuth.mCSRF = j.at("csrf").ToString();
        mRootAuth.mUserNSID = j.at("user").at("nsid").ToString();
        //SINFO("NSID: %s", userId.c_str());
    }

    std::string user = ParseFor(resp, "\"username\":\"(.*)\",\"realname\"");
    if (!user.empty())
    {
        mRootAuth.mUserName = user;
    }
    if (mRootAuth.mSignedIn)
    {
        SUCC("Authorization:\n\tSigned in : %s\n\tAPP Key   : %s\n\tCSRF      : %s\n\tUser      : %s\n\tUser NSID : %s", mRootAuth.mSignedIn ? "true" : "false", mRootAuth.mAppKey.c_str(), mRootAuth.mCSRF.c_str(), mRootAuth.mUserName.c_str(), mRootAuth.mUserNSID.c_str());
    }
    else
    {
        mRootAuth.Reset();
        SWARN("User not signed in.");
    }
}

void LF::Flickrd2::LoadApiKey()
{
    STATE(InternalState_t::LoadApiKey);

    mRootAuth.Reset();
    mGet = std::make_shared<LF::www::UrlGet>();
    mGet->SetCookies(mCookies);
    
    GetData* data = new GetData();
    data->mGet = mGet;
    
    CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
    mGet->Get("https://www.flickr.com/", true, data);
}

void LF::Flickrd2::ListFriendUsers()
{
    STATE(InternalState_t::ListContacts);
    if (mRootAuth.mSignedIn)
    {
        std::stringstream request;
        request << "https://api.flickr.com/services/rest?" << "method=flickr.contacts.getList" << "&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey;

        mGet = std::make_shared<LF::www::UrlGet>();
        
        mGet->SetCookies(mCookies);
        GetData* data = new GetData();
        mGet->SetLogFile("C:/temp/fd2/get.log");
        mGet->SetLogging(true, true);
        data->mGet = mGet;
        data->mRequest = request.str();

        CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
        mGet->Get(request.str(), true, data);
    }
    else
    {
        SWARN("ListFriendUsers: User not signed in.");
    }
}

void LF::Flickrd2::SetState(InternalState_t state)
{
    auto toStr = [](InternalState_t state) -> std::string 
    {
        switch (state)
        {
        case InternalState_t::IDLE:
            return "IDLE";
        case InternalState_t::LoadCookies:
            return "LoadCookies";
        case InternalState_t::LoadApiKey:
            return "LoadApiKey";
        }
        return "";
    };
    SINFO("State %s -> %s", toStr(mState).c_str(), toStr(state).c_str());
    mState = state;
}
