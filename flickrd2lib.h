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

//class Downloader
//{
//public:
//
//private:
//    std::queue<std::shared_ptr<LF::www::UrlGetDownload
//};
class JOB
{
public:
    JOB() {};

    bool Valid() const { return mValid; }
    const std::string& GetPath() const { return mPath; }
    std::string ToString()
    {
        std::stringstream ss;
        if (mValid)
        {
            ss << "JOB:" << mUserNsid << ":" << (mAlbumId.empty() ? "STREAM" : "ALBUM:") << mAlbumId;
            if (!mPath.empty())
            {
                ss << " -> " << mPath;
            }
        }
        else
            ss << "JOB:INVALID";
        return ss.str();
    }
private:

    bool mValid{ false };
    std::string mUserNsid;
    std::string mAlbumId;
    std::string mPath;

    friend class Flickrd2;
};

class PhotoInfo
{
public:
    PhotoInfo(std::string& url, std::string& name, std::string& filename) :
        mUrl(url),
        mName(name),
        mFilename(filename){}
    std::string mUrl;
    std::string mName;
    std::string mFilename;
};

class PhotoList
{
public:
    PhotoList() {}
    std::list<PhotoInfo> GetUrlsToLargestPhotos() const
    {
        std::list<PhotoInfo> ret;

        bool isAlbum = !mAlbumName.empty();
        auto childNodeName = isAlbum ? "photoset" : "photos";

        for (pugi::xml_node c = mDoc.child("rsp").child(childNodeName).child("photo"); c; c = c.next_sibling("photo"))
        {
            std::string sizeStr[] = { "6k", "5k", "4k", "3k", "o", "k", "h", "l"};
            for (int i = 0; i < sizeof(sizeStr) / sizeof(std::string); ++i)
            {
                std::stringstream ss;
                ss << "url_" << sizeStr[i];
                std::string url = c.attribute(ss.str().c_str()).as_string();
                std::string name = c.attribute("id").as_string();
                std::string filename;
                for (int i = url.size() - 1; i >= 0; --i)
                {
                    if (url.at(i) == '/')
                    {
                        filename = url.substr(i + 1);
                        break;
                    }
                }
                if (!url.empty())
                {
                    //std::cout << url << " : " << filename << std::endl;
                    ret.push_back({url, name, filename});
                    break;
                }
            }
        }
        return ret;
    };
    int Count()
    {
        return mCount;
    }
    std::string GetAlbumId() const { return mAlbumId; }
    std::string GetAlbumName() const { return mAlbumName; }
    std::string GetUserName() const { return mUserName; }
    std::string GetNSID() const { return mNSID; }

private:
    void SetDoc(pugi::xml_document& doc)
    {
        mDoc.reset(doc);
        //mDoc.save(std::cout);
        mAlbumId = mDoc.child("rsp").child("photoset").attribute("id").as_string();
        mAlbumName = mDoc.child("rsp").child("photoset").attribute("title").as_string();
        bool isAlbum = !mAlbumName.empty();
        auto childNodeName = isAlbum ? "photoset" : "photos";
        mCount = std::distance(mDoc.child("rsp").child(childNodeName).children("photo").begin(), mDoc.child("rsp").child(childNodeName).children("photo").end());
        mNSID = mDoc.child("rsp").child(childNodeName).attribute("owner").as_string();
        mUserName = mDoc.child("rsp").child(childNodeName).attribute("ownername").as_string();
    }
    pugi::xml_document mDoc;
    std::string mAlbumId;
    std::string mAlbumName;
    std::string mNSID;
    std::string mUserName;
    uint32_t mCount{ 0 };

    friend class Flickrd2;
};

class PhotoDownloader
{
public:
    PhotoDownloader(PhotoList& list) : mList(list)
    {}
    void Download(std::string homeDir);
private:
    PhotoList& mList;
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
    PhotoList ListUserAlbumPhotos(const std::string& userName, const std::string& albumName);
    PhotoList ListUserPhotos(const std::string& userName);
    std::string GetUsersNSID(const std::string& userName = "");

    void FixFlickrFolder(std::string path);
    bool SetFlickrFolder(std::string path);

    JOB GetFlickrJob(std::string jobFile);
    JOB GetFlickrJobFromUser(std::string username, std::string albumName = "");
    JOB GetFlickrJobFromUrl(std::string url);

    PhotoList ParseJob(const JOB& job);

    static bool CreateProjectFile(pugi::xml_document&, const std::string& nsid, const std::string& username, const std::string& albumId = "", const std::string& albumName = "");
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
    void OnRequestComplete_GetJobFromUrl(void* data);

    enum class InternalState_t
    {
        IDLE,
        LoadCookies,
        LoadApiKey,
        ListContacts,
        ListAlbums,
        LoadAlbumPhotos,
        LoadAllPhotos,
        GetUserNsid,
        GetUserInfo,
        GetJOBfromUrl,
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
    std::map<std::string, pugi::xml_document> mAlbumId2Photos;
    std::map<std::string, pugi::xml_document> mUserId2Photos; // stream photos

    std::shared_ptr<NetworkCookies> mCookies;
    std::shared_ptr<LF::www::UrlGet> mGet;

    void SetState(InternalState_t state);
    std::mutex mStateLock;
    std::atomic<InternalState_t> mState;

    bool mVerbose{ true };
    bool mQuiet{ false };

    struct
    {
        LF::fs::Directory mDir;
        bool mValid{ false };
    } mFlickrdDirectory;

    struct
    {
        std::string string;
        std::string string2;
        uint64_t number;
        void Reset()
        {
            string = "";
            string2 = "";
            number = 0;
        }
    } mUrlGetExchange;

    friend class StateGuard;
};

}