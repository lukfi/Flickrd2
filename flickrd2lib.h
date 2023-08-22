#include "networkcookies.h"
#include "urlget.h"

#include <memory>

namespace LF {

enum class Browser_t
{
    Opera,
    Vivaldi,
    Firefox
};

class Flickrd2
{
public:
    Flickrd2()
    {}

    void LoadCookies(Browser_t browser);
    bool LoadApiKey();

private:
    std::shared_ptr<NetworkCookies> mCookies;
    std::shared_ptr<LF::www::UrlGet> mGet;
};

}