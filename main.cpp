#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#include "flickrd2lib.h"
#include "fs/file.h"

int main(int argc, char *argv[])
{
    std::string homeDir = LF::fs::Directory::GetCurrentDir();
    //SDEB(homeDir.c_str());
    //SDEB(argv[0]);

    LF::Flickrd2 f2;
    LF::JOB j;

    f2.LoadCookies(LF::Browser_t::Firefox);
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
        return -1;
    }

    LF::PhotoList photostream = f2.ParseJob(j);
    printf(">>>>> %d photos %llu urls %s\n", photostream.Count(), photostream.GetUrlsToLargestPhotos().size(), photostream.GetAlbumName().c_str());
    system("pause");
    //f2.SetFlickrFolder("D:/fd");
    auto urls = photostream.GetUrlsToLargestPhotos();
    LF::www::UrlGetDownload d;
    int i = 0;
    for (auto& url : urls)
    {
        std::stringstream ss;
        ss << homeDir << "/" + photostream.GetUserName() << "/" << photostream.GetAlbumName() + "/" << url.mName << ".jpeg";
        SDEB("Downloading: %s", ss.str().c_str());
        if (LF::fs::File::Exists(ss.str()))
        {
            SDEB("File exists: %s", ss.str().c_str());
            continue;
        }

        while (!d.Download(url.mUrl, ss.str()))
        {
            d.WaitForAll();
        }
    }
    d.WaitForAll();
    return 0;
}