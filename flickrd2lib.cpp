#include "flickrd2lib.h"
#include "browserhacker.h"
#include "utils/datetime.h"
#include "regexp/regexp.h"

#define ENABLE_SDEBUG
#define DEBUG_PREFIX "Flickrd2: "
#include "utils/screenlogger.h"

void ParseFor(const std::string& str, const std::string& exp)
{
    //std::string exp = "\"csrf\":\"(.*)\"";
    LF::re::RegExp reg(exp);
    int r = reg.FindAll(str);
    SDEB("FOUND: %d", r);
    for (int i = 0; i < r; ++i)
    {
        SINFO("%d -> %s", i, reg.Cap(i).c_str());
    }
}

class GetData
{
public:
    std::shared_ptr<LF::www::UrlGet> mGet;
};

void LF::Flickrd2::LoadCookies(Browser_t browser)
{
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
    ParseFor(data->mGet->GetResponse().mCurrentResponse, "\"csrf\":\"(.*)\"");
}

bool LF::Flickrd2::LoadApiKey()
{
    mGet = std::make_shared<LF::www::UrlGet>();
    mGet->SetCookies(mCookies);
    
    GetData* data = new GetData();
    data->mGet = mGet;
    
    CONNECT(mGet->REQUEST_COMPLETE, OnRequestComplete);
    mGet->Get("https://www.flickr.com/", true, data);
    return false;
}
