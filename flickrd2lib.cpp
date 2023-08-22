#include "flickrd2lib.h"
#include "browserhacker.h"

#define ENABLE_SDEBUG
#define DEBUG_PREFIX "Flickrd2: "
#include "utils/screenlogger.h"

void LF::Flickrd2::LoadCookies(Browser_t browser)
{
    BrowserHacker hacker(BrowserHacker::Browser_t::Vivaldi);
    if (hacker.BrowserValid())
    {
        auto cookies = hacker.GetCookies("%flickr.com");
        SUCC("Loaded cookies: %d", cookies.Get().size());
    }
    else
    {
        SWARN("Failed to load cookies");
    }
}
