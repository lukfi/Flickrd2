#include "flickrd2lib.h"
#include "browserhacker.h"
#include "utils/datetime.h"
#include "regexp/regexp.h"

namespace Fd2 {
#include "utils/SimpleJSON/json.hpp"
};

#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#if 0
#ifdef SDEB
#undef SDEB
#define SDEB(...)
#endif
#endif 

#define STATE(x) StateGuard s(this, x); if (!s.Valid()) return

static bool IsNSID(const std::string& nsid)
{
    std::regex pattern("[0-9]+@N[0-9]{2}");
    if (std::regex_match(nsid, pattern))
    {
        return true;
    }
    return false;
}

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
    std::string mNSID;
    std::string mAlbumId;
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
        std::string cookieSession;// = "118205923%3A31d9e76722e34be9e4ef8aaaf78faf51";

        if (false)
        {
            for (auto& c : mCookies->Get())
            {
                std::cout << c.second->GetName() << " :: " << c.second->GetValue() << std::endl;
            }
        }

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
    case InternalState_t::ListContacts:
        OnRequestComplete_ListContacts(data->mGet->GetResponse().mCurrentResponse);
        break;
    case InternalState_t::ListAlbums:
        OnRequestComplete_ListAlbums(data);
        break;
    case InternalState_t::LoadAlbumPhotos:
    case InternalState_t::LoadAllPhotos:
        OnRequestComplete_ListAlbumPhotos(data);
        break;
    case InternalState_t::GetUserNsid:
        OnRequestComplete_GetNsid(data->mGet->GetResponse().mCurrentResponse);
        break;
    case InternalState_t::GetUserInfo:
        OnRequestComplete_GetUserInfo(data);
        break;
    case InternalState_t::GetJOBfromUrl:
        OnRequestComplete_GetJobFromUrl(data);
        break;
    default:
        PRINT("===== RESPONSE ====\n");
        std::cout << data->mGet->GetResponse().mCurrentResponse;
        PRINT("\n===================\n", data->mGet->GetResponse().mCurrentResponse.c_str());
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

void LF::Flickrd2::OnRequestComplete_ListContacts(const std::string& resp)
{
    //SDEB("%>>>>>>\n%s>>>>>>>>", resp.c_str());
    if (mContacts.mContacts.load_string(resp.c_str()))
    {
        auto contacts = mContacts.mContacts.child("rsp").child("contacts");
        SDEB("Contacts parsed OK count: %d", contacts.attribute("total").as_int());
        for (pugi::xml_node c = contacts.child("contact"); c; c = c.next_sibling("contact"))
        {
            //PRINT("-> %s [%s] (%s)\n", c.attribute("username").as_string(), c.attribute("realname").as_string(), c.attribute("nsid").as_string());
        }
        mContacts.mLoaded = true;
    }
    else
    {
        SWARN("Failed to parse contacts.");
    }
}

void LF::Flickrd2::OnRequestComplete_ListAlbums(void* data)
{
    GetData* getData = reinterpret_cast<GetData*>(data);
    std::string resp = getData->mGet->GetResponse().mCurrentResponse;
    if (mContacts.mNSID2AlbumList.find(getData->mNSID) == mContacts.mNSID2AlbumList.end())
    {
        SDEB("Creating Album list for %s", getData->mNSID.c_str());
        mContacts.mNSID2AlbumList[getData->mNSID] = pugi::xml_document();
    }
    else
    {
        SDEB("Album list for %s exists", getData->mNSID.c_str());
        if (mUrlGetExchange.number == 1)
        {
            SDEB("Reseting old album list");
            mContacts.mNSID2AlbumList[getData->mNSID].reset();
        }
    }
    pugi::xml_document& userAlbumList = mContacts.mNSID2AlbumList[getData->mNSID];
    pugi::xml_document tempAlbums;
    pugi::xml_document* albums = (mUrlGetExchange.number == 1) ? &userAlbumList : &tempAlbums;
    if (albums->load_string(resp.c_str()))
    {
        auto photosets = albums->child("rsp").child("photosets");

        SDEB("Photosets parsed OK total count: %d, page: %s/%s", photosets.attribute("total").as_int(), photosets.attribute("page").as_string(), photosets.attribute("pages").as_string());
        int page = photosets.attribute("page").as_int(-1);
        int pages = photosets.attribute("pages").as_int(-1);
        if (page == -1 || pages == -1 || page != mUrlGetExchange.number)
        {
            SERR("OnRequestComplete_ListAlbums: wrong page");
            mUrlGetExchange.number = 0;
            return;
        }
        
        if (page > 1)
        {
            pugi::xml_node photosets = userAlbumList.child("rsp").child("photosets");
            for (pugi::xml_node child = tempAlbums.child("rsp").child("photosets").child("photoset");
                                child;
                                child = child.next_sibling("photoset"))
            {
                //SDEB("C -> %s", child.attribute("id").as_string());
                //SDEB("C -> %s = %s", child.attribute("id").as_string(), child.child_value("title"));
                //userAlbumList.child("rsp").child("photosets").append_child(child);
                photosets.append_copy(child);
            }
        }

        SDEB("Album Count: %d", std::distance(userAlbumList.child("rsp").child("photosets").children("photoset").begin(), userAlbumList.child("rsp").child("photosets").children("photoset").end()));

        if (page < pages)
        {
            ++page;
            SDEB("Need new request for page: %d", page);
            mUrlGetExchange.number = page;
        }
        else
        {
            SDEB("Request finalised");
            mUrlGetExchange.number = 0;

            int i = 0;
            for (pugi::xml_node c = userAlbumList.child("rsp").child("photosets").child("photoset"); c; c = c.next_sibling("photoset"))
            {
                ++i;
                PRINT("[% 4d] %s => %s\n", i, c.child_value("title"), c.attribute("id").as_string());
            }
        }
    }
    else
    {
        SWARN("Failed to parse albums.");
    }
}

void LF::Flickrd2::OnRequestComplete_ListAlbumPhotos(void* data)
{
    GetData* getData = reinterpret_cast<GetData*>(data);
    std::string resp = getData->mGet->GetResponse().mCurrentResponse;
    bool parsingAlbum = !getData->mAlbumId.empty();

    if (parsingAlbum)
    {
        if (mAlbumId2Photos.find(getData->mAlbumId) == mAlbumId2Photos.end())
        {
            SDEB("Creating Album Photos list for %s", getData->mAlbumId.c_str());
            mAlbumId2Photos[getData->mAlbumId] = pugi::xml_document();
        }
        else
        {
            SDEB("Album Photos list for %s exists", getData->mAlbumId.c_str());
            if (mUrlGetExchange.number == 1)
            {
                SDEB("Reseting old album photos list");
                mAlbumId2Photos[getData->mAlbumId].reset();
            }
        }
    }
    else
    {
        if (mUserId2Photos.find(getData->mNSID) == mUserId2Photos.end())
        {
            SDEB("Creating User Photos list for %s", getData->mNSID.c_str());
            mUserId2Photos[getData->mNSID] = pugi::xml_document();
        }
        else
        {
            SDEB("Photos list for %s exists", getData->mNSID.c_str());
            if (mUrlGetExchange.number == 1)
            {
                SDEB("Reseting old photos list");
                mUserId2Photos[getData->mNSID].reset();
            }
        }
    }
    pugi::xml_document& userPhotosList = parsingAlbum ? mAlbumId2Photos[getData->mAlbumId] : mUserId2Photos[getData->mNSID];
    pugi::xml_document tempList;
    pugi::xml_document* photos = (mUrlGetExchange.number == 1) ? &userPhotosList : &tempList;

    if (photos->load_string(resp.c_str()))
    {
        auto childName = parsingAlbum ? "photoset" : "photos";
        auto photoset = photos->child("rsp").child(childName);

        SINFO("Photoset parsed OK total count: %d, page: %s/%s", photoset.attribute("total").as_int(), photoset.attribute("page").as_string(), photoset.attribute("pages").as_string());
        int page = photoset.attribute("page").as_int(-1);
        int pages = photoset.attribute("pages").as_int(-1);
        if (page == -1 || pages == -1 || page != mUrlGetExchange.number)
        {
            SERR("OnRequestComplete_ListAlbumPhotos: wrong page");
            mUrlGetExchange.number = 0;
            return;
        }

        if (page > 1)
        {
            pugi::xml_node photoset2 = userPhotosList.child("rsp").child(childName);
            for (pugi::xml_node child = tempList.child("rsp").child(childName).child("photo");
                child;
                child = child.next_sibling("photo"))
            {
                //SDEB("C -> %s", child.attribute("id").as_string());
                //SDEB("C -> %s = %s", child.attribute("id").as_string(), child.child_value("title"));
                //userAlbumList.child("rsp").child(childName).append_child(child);
                photoset2.append_copy(child);
            }
        }

        SINFO("Photos Count: %d", std::distance(userPhotosList.child("rsp").child(childName).children("photo").begin(), userPhotosList.child("rsp").child(childName).children("photo").end()));

        if (page < pages)
        {
            ++page;
            SINFO("Need new request for page: %d", page);
            mUrlGetExchange.number = page;
        }
        else
        {
            SINFO("Request finalised");
            mUrlGetExchange.number = 0;

            int i = 0;
            for (pugi::xml_node c = userPhotosList.child("rsp").child(childName).child("photo"); c; c = c.next_sibling("photo"))
            {
                ++i;
                PRINT("[% 4d] %s = %s\n", i, c.attribute("title").as_string(), c.attribute("url_4k").as_string());
            }
        }
    }
    else
    {
        SWARN("Failed to parse album photos.");
    }
}

void LF::Flickrd2::OnRequestComplete_GetNsid(const std::string& resp)
{
    pugi::xml_document doc;
    if (doc.load_string(resp.c_str()))
    {
        std::string nsid = doc.child("rsp").child("user").attribute("nsid").as_string();
        mUrlGetExchange.string = nsid;
        SDEB("NSID: %s", nsid.c_str());
    }
}

void LF::Flickrd2::OnRequestComplete_GetUserInfo(void* data)
{
    GetData* getData = reinterpret_cast<GetData*>(data);
    std::string resp = getData->mGet->GetResponse().mCurrentResponse;
    pugi::xml_document doc;
    doc.load_string(resp.c_str());

    if (doc.child("rsp").attribute("stat").as_string() == std::string("ok"))
    {
        //mNSID2UserCache[getData->mNSID] = pugi::xml_document();
        mNSID2UserCache[getData->mNSID] = std::move(doc);
    }
}

void LF::Flickrd2::OnRequestComplete_GetJobFromUrl(void* data)
{
    GetData* getData = reinterpret_cast<GetData*>(data);
    std::string resp = getData->mGet->GetResponse().mCurrentResponse;
    LF::re::RegExp reg("\"nsid\":\"([0-9]+@N[0-9]+)\"");
    int r = reg.FindAll(resp);
    bool found = false;
    for (int i = 0; i < r && !found; ++i)
    {
        //std::cout << "CAP(" << i << "): " << str << std::endl;
        auto str = reg.Cap(i);
        if (IsNSID(str) && str != mRootAuth.mUserNSID)
        {
            mUrlGetExchange.string = str;
            found = true;
        }
    }

    LF::re::RegExp reg2("albumId_([0-9]*)_");
    int r2 = reg2.FindAll(resp);
    found = false;
    for (int i = 0; i < r2 && !found; ++i)
    {
        auto str = reg2.Cap(i);
        if (std::all_of(str.begin(), str.end(), [](char ch) { return std::isdigit(ch); }))
        {
            //std::cout << "CAP(" << i << "): " << str << std::endl;
            mUrlGetExchange.string2 = str;
            found = true;
        }
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
    //mGet->SetLogging(true, true);
    CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
    mGet->Get("https://www.flickr.com/", true, data);
}

void LF::Flickrd2::ListFriendUsers()
{
    STATE(InternalState_t::ListContacts);
    if (mRootAuth.mSignedIn)
    {
        std::stringstream request;
        request << "https://api.flickr.com/services/rest?" << "method=flickr.contacts.getList" << "&format=rest&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey;

        mGet = std::make_shared<LF::www::UrlGet>();
        
        mGet->SetCookies(mCookies);
        GetData* data = new GetData();
        //mGet->SetLogFile("C:/temp/fd2/get.log");
        //mGet->SetLogging(true, true);
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

void LF::Flickrd2::ListUsersAlbums(const std::string& userName)
{
    std::string userNsid = GetUsersNSID(userName);

    SDEB("%s is %s", userName.c_str(), userNsid.c_str());

    STATE(InternalState_t::ListAlbums);
    if (mRootAuth.mSignedIn)
    {
        std::stringstream request;
        request << "https://api.flickr.com/services/rest?" << "method=flickr.photosets.getList" << "&format=rest&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey;
        if (!userNsid.empty())
        {
            request << "&user_id=" << userNsid;
        }

        mGet = std::make_shared<LF::www::UrlGet>();

        mGet->SetCookies(mCookies);
        //mGet->SetLogFile("C:/temp/fd2/ListUsersAlbums.log");
        //mGet->SetLogging(true, true);
        CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
        mUrlGetExchange.number = 1;
        do
        {
            GetData* data = new GetData();
            data->mGet = mGet;
            data->mNSID = userNsid;
            std::stringstream currentRequest;
            currentRequest << request.str() << "&page=" << mUrlGetExchange.number;
            data->mRequest = currentRequest.str();
            mGet->Get(currentRequest.str(), true, data);
        } while (mUrlGetExchange.number);
    }
    else
    {
        SWARN("ListUsersAlbums: User not signed in.");
    }
}

LF::PhotoList LF::Flickrd2::ListUserAlbumPhotos(const std::string& userName, const std::string& albumName)
{
    PhotoList retList;

    std::string nsid = GetUsersNSID(userName);
    if (nsid.empty())
    {
        // TODO: warning
        SERR("NSID is empty");
        return retList;
    }
    std::string albumID = GetUsersAlbumId(nsid, albumName);
    SDEB("Album ID: %s", albumID.c_str());

    STATE(InternalState_t::LoadAlbumPhotos) retList;

    if (!albumID.empty() && mRootAuth.mSignedIn)
    {
        std::stringstream request;
        request << "https://api.flickr.com/services/rest?" << "method=flickr.photosets.getPhotos" << "&format=rest&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey << "&photoset_id=" << albumID;

        // extras
        request << "&extras=url_3k,url_4k,url_o,url_l";

        if (!nsid.empty())
        {
            request << "&user_id=" << nsid;
        }

        mGet = std::make_shared<LF::www::UrlGet>();

        mGet->SetCookies(mCookies);
        ////mGet->SetLogFile("C:/temp/fd2/ListUsersAlbums.log");
        ////mGet->SetLogging(true, true);
        CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);

        mUrlGetExchange.number = 1;
        do
        {
            GetData* data = new GetData();
            data->mGet = mGet;
            data->mNSID = nsid;
            data->mAlbumId = albumID;
            std::stringstream currentRequest;
            currentRequest << request.str() << "&page=" << mUrlGetExchange.number;
            data->mRequest = currentRequest.str();
            mGet->Get(currentRequest.str(), true, data);
        } while (mUrlGetExchange.number);

        auto& d = mAlbumId2Photos[albumID];
        retList.SetDoc(d);

        return retList;
    }
    else
    {
        SWARN("ListUserAlbumPhotos: User not signed in.");
    }
    return retList;
}

LF::PhotoList LF::Flickrd2::ListUserPhotos(const std::string& userName)
{
    PhotoList retList;

    std::string nsid = GetUsersNSID(userName);
    if (nsid.empty())
    {
        // TODO: warning
        SERR("NSID is empty");
        return retList;
    }

    STATE(InternalState_t::LoadAllPhotos) retList;

    if (mRootAuth.mSignedIn)
    {
        std::stringstream request;
        request << "https://api.flickr.com/services/rest?" << "method=flickr.people.getPhotos" << "&format=rest&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey;

        // extras
        request << "&extras=url_3k,url_4k,url_o,url_l";

        if (!nsid.empty())
        {
            request << "&user_id=" << nsid;
        }

        mGet = std::make_shared<LF::www::UrlGet>();

        mGet->SetCookies(mCookies);
        CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);

        mUrlGetExchange.number = 1;
        do
        {
            GetData* data = new GetData();
            data->mGet = mGet;
            data->mNSID = nsid;
            //data->mAlbumId = albumID;
            std::stringstream currentRequest;
            currentRequest << request.str() << "&page=" << mUrlGetExchange.number;
            data->mRequest = currentRequest.str();
            mGet->Get(currentRequest.str(), true, data);
        } while (mUrlGetExchange.number);

        auto& d = mUserId2Photos[nsid];
        retList.SetDoc(d);

        return retList;
    }


    return retList;
}

std::string LF::Flickrd2::GetUsersNSID(const std::string& userName)
{
    if (userName.empty()) // self
    {
        return userName;
    }

    if (IsNSID(userName))
    {
        SINFO("%s is already an NSID", userName.c_str());
        return userName;
    }

    std::string nsid;
    if (mContacts.mLoaded)
    {
        auto contacts = mContacts.mContacts.child("rsp").child("contacts");
        for (pugi::xml_node c = contacts.child("contact"); c; c = c.next_sibling("contact"))
        {
            if (c.attribute("username").as_string() == userName ||
                c.attribute("realname").as_string() == userName)
            {
                nsid = c.attribute("nsid").as_string();
                SDEB("-> %s nsid: %s", userName.c_str(), c.attribute("nsid").as_string());
            }
        }
    }
    if (nsid.empty())
    {
        STATE(InternalState_t::GetUserNsid) nsid;
        if (mRootAuth.mSignedIn)
        {
            std::stringstream request;
            request << "https://api.flickr.com/services/rest?" << "method=flickr.people.findByUsername" << "&format=rest&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey << "&username=" << userName;

            mGet = std::make_shared<LF::www::UrlGet>();

            mGet->SetCookies(mCookies);
            GetData* data = new GetData();
            //mGet->SetLogFile("C:/temp/fd2/get.log");
            //mGet->SetLogging(true, true);
            data->mGet = mGet;
            data->mRequest = request.str();

            CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
            mGet->Get(request.str(), true, data);
            SDEB("NSID is: %s", mUrlGetExchange.string.c_str());
            nsid = mUrlGetExchange.string;
        }
        else
        {
            SWARN("ListFriendUsers: User not signed in.");
        }
    }
    return nsid;
}

void LF::Flickrd2::FixFlickrFolder(std::string path)
{
    LF::fs::Directory mainDir(path);
    if (!mainDir.Exists())
    {
        SWARN("Fix: Directory doesn't exist: %s", path.c_str());
    }
    else
    {
        SDEB("Fix: Checking main dir: %s", mainDir.PWD().c_str());
    }

    auto ConvertVer1To2 = [this](pugi::xml_document& flickr) -> bool
    {
        std::string userId = flickr.child("flickrd").child("task").attribute("userId").as_string();
        if (userId.empty())
        {
            return false;
        }
        else
        {
            std::string nsid = GetUsersNSID(userId);
            SINFO("Fix %s -> %s", userId.c_str(), nsid.c_str());
            if (!nsid.empty())
            {
                //flickr.child("flickrd").child("task").attribute("nsid").set_value(nsid.c_str());
                pugi::xml_document* info = LoadUserInfo(nsid);
                if (info)
                {
                    flickr.child("flickrd").child("task").append_attribute("nsid").set_value(nsid.c_str());
                    flickr.child("flickrd").child("task").append_attribute("username").set_value(info->child("rsp").child("person").child_value("username"));
                    flickr.child("flickrd").child("task").append_attribute("realname").set_value(info->child("rsp").child("person").child_value("realname"));
                    flickr.child("flickrd").child("task").remove_attribute("userId");
                    flickr.child("flickrd").child("task").remove_attribute("userName");
                    std::stringstream ss;
                    flickr.save(ss);
                    SDEB(ss.str().c_str());
                }
            }
            return true;
        }
    };

    auto FixSubfolder = [ConvertVer1To2](const std::string& subfolder) -> bool
    {
        LF::fs::File f(subfolder + "/.flickrd");
        SDEB("Fixing: %s", f.GetFullPath().c_str());
        if (f.Exists() && f.Open())
        {
            std::string fileStr = f.GetAsString();
            f.Close();
            pugi::xml_document flickr;
            flickr.load_string(fileStr.c_str());

            SDEB(flickr.child("flickrd").attribute("version").as_string());
            if (flickr.child("flickrd").attribute("version").as_string() == std::string("1"))
            {
                return ConvertVer1To2(flickr);
            }
        }
        return true;
    };

    auto subdirs = mainDir.GetSubdirs();
    for (auto& sd : subdirs)
    {
        if (sd.Type == LF::fs::FileSystemElement::Directory)
        {
            FixSubfolder(mainDir.PWD() + "/" + sd.Name);
        }
    }
}

bool LF::Flickrd2::SetFlickrFolder(std::string path)
{
    auto listFolderJobs = [](LF::fs::Directory& dir)
    {
        auto subdirs = dir.GetSubdirs();
        for (auto& sd : subdirs)
        {
            if (sd.Type == LF::fs::FileSystemElement::Directory)
            {
                
            }
        }
    };

    mFlickrdDirectory.mValid = false;
    if (mFlickrdDirectory.mDir.CD(path))
    {

    }
    return mFlickrdDirectory.mValid;
}

LF::JOB LF::Flickrd2::GetFlickrJob(std::string jobFile)
{
    bool ok = false;
    bool isPath = false;
    std::string pathDir;
    pugi::xml_document flickr;
    if (!flickr.load_string(jobFile.c_str()))
    {
        std::string path = jobFile;
        LF::fs::File f(path);
        if (f.Exists() && f.Open())
        {
            if (flickr.load_string(f.GetAsString().c_str()))
            {
                SINFO("LOADED JOB from path: %s", jobFile.c_str());
                isPath = true;
                pathDir = f.GetDirectoryPath();
                ok = true;
            }
            else
            {
                SERR("Cant load JOB");
            }
        }
    }
    else
    {
        ok = true;
        SINFO("LOADED JOB from string");
    }

    if (ok)
    {
        LF::JOB job;
        auto task = flickr.child("flickrd").child("task");
        job.mUserNsid = task.attribute("userId").as_string();
        if (task.attribute("type").as_string() == std::string("album"))
        {
            job.mAlbumId = task.attribute("albumId").as_string();
        }
        if (!IsNSID(job.mUserNsid))
        {
            job.mUserNsid = GetUsersNSID(job.mUserNsid);
        }
        job.mValid = IsNSID(job.mUserNsid);
        if (isPath)
        {
            job.mPath = pathDir;
        }
        return job;
    }

    return LF::JOB();
}

LF::JOB LF::Flickrd2::GetFlickrJobFromUser(std::string username, std::string albumName)
{
    LF::JOB j;
    j.mUserNsid = GetUsersNSID(username);
    j.mValid = IsNSID(j.mUserNsid);
    return j;
}

LF::JOB LF::Flickrd2::GetFlickrJobFromUrl(std::string url)
{
    LF::JOB job;
    LF::www::Url u(url);
    if (u.IsValid() && u.GetHostname() == "flickr.com")
    {
        STATE(InternalState_t::GetJOBfromUrl) job;

        mUrlGetExchange.Reset();
        mGet = std::make_shared<LF::www::UrlGet>();

        mGet->SetCookies(mCookies);
        GetData* data = new GetData();
        data->mGet = mGet;
        SINFO("REQ: %s", url.c_str());
        CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
        mGet->Get(url, true, data);

        job.mUserNsid = mUrlGetExchange.string;
        job.mAlbumId = mUrlGetExchange.string2;
        job.mValid = IsNSID(job.mUserNsid);
    }
    return job;
}

LF::PhotoList LF::Flickrd2::ParseJob(const JOB& job)
{
    if (job.mValid)
    {
        if (job.mAlbumId.empty()) // stream
        {
            return ListUserPhotos(job.mUserNsid);
        }
        else
        {
            return ListUserAlbumPhotos(job.mUserNsid, job.mAlbumId);
        }
    }
    else
    {
        SERR("JOB invalid");
    }
    return PhotoList();
}

std::string LF::Flickrd2::GetUsersAlbumId(const std::string& nsid, const std::string& albumName)
{
    if (mContacts.mNSID2AlbumList.find(nsid) != mContacts.mNSID2AlbumList.end())
    {
        SINFO("Already know the album list");
        auto& doc = mContacts.mNSID2AlbumList.find(nsid)->second;
        for (auto& ps : doc.child("rsp").child("photosets").children("photoset"))
        {
            if (ps.child_value("title") == albumName || ps.attribute("id").as_string() == albumName)
            {
                return ps.attribute("id").as_string();
            }
        }
    }
    else
    {
        ListUsersAlbums(nsid);

        auto& doc = mContacts.mNSID2AlbumList.find(nsid)->second;
        for (auto& ps : doc.child("rsp").child("photosets").children("photoset"))
        {
            if (ps.child_value("title") == albumName || ps.attribute("id").as_string() == albumName)
            {
                return ps.attribute("id").as_string();
            }
        }
    }
    return std::string();
}

pugi::xml_document* LF::Flickrd2::LoadUserInfo(const std::string& nsid)
{
    pugi::xml_document* ret = nullptr;

    auto infoIter = mNSID2UserCache.find(nsid);
    if (infoIter != mNSID2UserCache.end())
    {
        ret = &infoIter->second;
    }
    else
    {
        if (mRootAuth.mSignedIn)
        {
            STATE(InternalState_t::GetUserInfo) ret;

            std::stringstream request;
            request << "https://api.flickr.com/services/rest?" << "method=flickr.people.getInfo" << "&format=rest&csrf=" << mRootAuth.mCSRF << "&api_key=" << mRootAuth.mAppKey << "&user_id=" << nsid;

            mGet = std::make_shared<LF::www::UrlGet>();

            mGet->SetCookies(mCookies);
            GetData* data = new GetData();
            data->mGet = mGet;
            data->mRequest = request.str();
            data->mNSID = nsid;

            CONNECT(mGet->REQUEST_COMPLETE, Flickrd2, OnRequestComplete, this);
            mGet->Get(request.str(), true, data);
            SDEB("NSID is: %s", mUrlGetExchange.string.c_str());

            auto infoIter2 = mNSID2UserCache.find(nsid);
            if (infoIter2 != mNSID2UserCache.end())
            {
                ret = &infoIter2->second;
                SINFO("Got user info %s -> %s", ret->child("rsp").child("person").child_value("username"), ret->child("rsp").child("person").attribute("nsid").as_string());
            }
            else
            {
                SWARN("Info for user: %s not found", nsid.c_str());
            }
        }
    }

    return ret;
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
        case InternalState_t::ListContacts:
            return "ListContacts";
        case InternalState_t::ListAlbums:
            return "ListAlbums";
        case InternalState_t::LoadAlbumPhotos:
            return "LoadAlbumPhotos";
        case InternalState_t::LoadAllPhotos:
            return "LoadAllPhotos";
        default:
        {
            std::stringstream ss;
            ss << "Unknown[" << (int)state << "]";
            return ss.str();
        }
        }
        return "";
    };
    SINFO("State %s -> %s", toStr(mState).c_str(), toStr(state).c_str());
    mState = state;
}
