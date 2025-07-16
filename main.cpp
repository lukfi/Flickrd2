#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#include "flickrd2lib.h"
#include "fs/file.h"

int main(int argc, char *argv[])
{
    SINFO("ver.: 1.1");
    std::string homeDir = LF::fs::Directory::GetCurrentDir();
    //SDEB(homeDir.c_str());
    //SDEB(argv[0]);

    LF::Flickrd2 f2;
    LF::JOB j;

    f2.LoadCookies(LF::Browser_t::Vivaldi);
    f2.LoadApiKey();
    f2.ListFriendUsers();

    if (argc > 1)
    {
        LF::www::Url url(argv[1]);
        if (url.IsValid())
        {
            j = f2.GetFlickrJobFromUrl(argv[1]);
            if (!j.Valid())
            {
                SERR("No valid flickr job found form URL: %s", argv[1]);
                return -1;
            }
        }
    }

#if 1
    //std::list<std::string> jobFiles;
    if (argc > 1 && !j.Valid())
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string path = argv[i];
            j = f2.GetFlickrJob(path);
            //LF::fs::File f(path);
            //if (f.Exists() && f.Open())
            //{
            //
            //}
        }
    }
#endif
    //if (!j.Valid())
    //{
    //    j = f2.GetFlickrJobFromUser("");
    //}

    if (!j.Valid())
    {
        SERR("No valid flickr job found.");
        system("pause");
        return -1;
    }

    SINFO(j.ToString().c_str());
    LF::PhotoList photostream = f2.ParseJob(j);

    SINFO("Photos info: %s (%s): %d photos", photostream.GetUserName().c_str(), (photostream.GetAlbumName().empty() ? "stream" : photostream.GetAlbumName().c_str()), photostream.Count());
    system("pause");
    //f2.SetFlickrFolder("D:/fd");

    std::string photosDir;

    if (j.GetPath().empty())
    {
        std::stringstream ss;
        ss << homeDir << "/" + photostream.GetUserName() << "/";
        if (!photostream.GetAlbumName().empty())
        {
            ss << photostream.GetAlbumName();
        }
        photosDir = ss.str();
    }
    else
    {
        photosDir = j.GetPath();
    }

    LF::PhotoDownloader downloader(photostream);
    downloader.Download(photosDir);
    system("pause");
    return 0;
}