#include "networkcookies.h"
#include "urlget.h"
#include "xml/pugi/pugixml.h"

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
    void ListFriendUsers();
    void ListUsersAlbums(const std::string& userName = "");
    void ListUserAlbumPhotos(const std::string& userName, const std::string& albumName);
    std::string GetUsersNSID(const std::string& userName = "");

    void FixFlickrFolder(std::string path);
private:
    std::string GetUsersAlbumId(const std::string& nsid, const std::string& albumName);
    pugi::xml_document* LoadUserInfo(const std::string& nsid);

    void OnRequestComplete(void* userData, LF::www::UrlGet::RequestResult_t res);
    void OnRequestComplete_LoadApiKey(const std::string& resp);
    void OnRequestComplete_ListContacts(const std::string& resp);
    void OnRequestComplete_ListAlbums(void* data);
    void OnRequestComplete_ListAlbumPhotos(void* data);
    void OnRequestComplete_GetNsid(const std::string& resp);
    void OnRequestComplete_GetUserInfo(void* data);

    enum class InternalState_t
    {
        IDLE,
        LoadCookies,
        LoadApiKey,
        ListContacts,
        ListAlbums,
        LoadAlbumPhotos,
        GetUserNsid,
        GetUserInfo
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

    struct RootAuth
    {
        std::string mAppKey;
        std::string mCSRF;
        std::string mUserNSID;
        std::string mUserName;
        bool mSignedIn{ false };
        void Reset()
        {
            mAppKey.clear();
            mCSRF.clear();
            mUserNSID.clear();
            mUserName.clear();
            mSignedIn = false;
        }
    } mRootAuth;

    struct Contacts
    {
        bool mLoaded{ false };
        pugi::xml_document mContacts;
        std::map<std::string, pugi::xml_document> mNSID2AlbumList;
    } mContacts;

    std::map<std::string, pugi::xml_document> mNSID2UserCache;

    std::shared_ptr<NetworkCookies> mCookies;
    std::shared_ptr<LF::www::UrlGet> mGet;

    void SetState(InternalState_t state);
    std::mutex mStateLock;
    std::atomic<InternalState_t> mState;

    struct
    {
        std::string string;
        uint64_t number;
    } mUrlGetExchange;

    friend class StateGuard;
};

}