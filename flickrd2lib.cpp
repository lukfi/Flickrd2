#include "flickrd2lib.h"
#include "browserhacker.h"
#include "utils/datetime.h"
#include "regexp/regexp.h"

namespace Fd2 {
#include "utils/SimpleJSON/json.hpp"
};

#define ENABLE_SDEBUG
#define DEBUG_PREFIX "Flickrd2: "
#include "utils/screenlogger.h"

#define STATE(x) StateGuard s(this, x); if (!s.Valid()) return;

std::string ParseFor(const std::string& str, const std::string& exp)
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
};

void LF::Flickrd2::LoadCookies(Browser_t browser)
{
    STATE(InternalState_t::LoadCookies)

    BrowserHacker hacker(BrowserHacker::Browser_t::Vivaldi);
    if (hacker.BrowserValid())
    {
        mCookies = hacker.GetCookies("%flickr.com");
        SUCC("Loaded cookies: %d", mCookies->Get().size());
    }
    else
    {
        SWARN("Failed to load cookies");
    }
}

void OnRequestComplete(void* userData, LF::www::UrlGet::RequestResult_t res)
{
    SDEB("Complete: %d", res);
    GetData* data = reinterpret_cast<GetData*>(userData);

    LF::utils::DateTime now;
    now.Now();
    LF::fs::File f(std::string("C:/temp/fd2/") + now.ToString("file"));
    if (f.CreateOpen())
    {
        f.Put((uint8_t*)data->mGet->GetResponse().mCurrentResponse.data(), data->mGet->GetResponse().mCurrentResponse.size());
    }
    std::string siteKey = ParseFor(data->mGet->GetResponse().mCurrentResponse, "root\\.YUI_config\\.flickr\\.api\\.site_key = \"(.*)\";");
    SINFO("KEY: %s", siteKey.c_str());
    std::string ret = ParseFor(data->mGet->GetResponse().mCurrentResponse, "root\\.auth = (.*);");
    if (!ret.empty())
    {
        //SINFO(ret.c_str());
        Fd2::json::JSON j = Fd2::json::JSON::Load(ret);
        SINFO("SIGNED: %d", j.at("signedIn").ToBool());
    }
}

void LF::Flickrd2::LoadApiKey()
{
    STATE(InternalState_t::LoadApiKey)

    mGet = std::make_shared<LF::www::UrlGet>();
    mGet->SetCookies(mCookies);
    
    GetData* data = new GetData();
    data->mGet = mGet;
    
    CONNECT(mGet->REQUEST_COMPLETE, OnRequestComplete);
    mGet->Get("https://www.flickr.com/", true, data);
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
    };
    SINFO("State %s -> %s", toStr(mState).c_str(), toStr(state).c_str());
    mState = state;
}
