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
    Flickrd2() : mState(InternalState_t::IDLE)
    {}

    void LoadCookies(Browser_t browser);
    void LoadApiKey();

private:
    enum class InternalState_t
    {
        IDLE,
        LoadCookies,
        LoadApiKey
    };

    class StateGuard
    {
    public:
        StateGuard(Flickrd2* app, InternalState_t state) :
            mApp(app)
        {
            std::lock_guard<std::mutex> lock(mApp->mStateLock);
            if (mApp->mState == InternalState_t::IDLE)
            {
                mApp->SetState(state);
                mValid = true;
            }
        }
        ~StateGuard()
        {
            if (mValid)
            {
                std::lock_guard<std::mutex> lock(mApp->mStateLock);
                mApp->SetState(InternalState_t::IDLE);
            }
        }
        bool Valid() const { return mValid; }
    private:
        Flickrd2* mApp;
        bool mValid {false};
    };

    std::shared_ptr<NetworkCookies> mCookies;
    std::shared_ptr<LF::www::UrlGet> mGet;

    void SetState(InternalState_t state);
    std::mutex mStateLock;
    std::atomic<InternalState_t> mState;

    friend class StateGuard;
};

}