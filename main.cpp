#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#include "flickrd2lib.h"

int main(/*int argc, wchar_t *argv[]*/)
{
    LF::Flickrd2 f2;

    f2.LoadCookies(LF::Browser_t::Vivaldi);
    f2.LoadApiKey();
    //f2.ListFriendUsers();
    //f2.GetUsersNSID("beemjessie");
    //f2.GetUsersNSID("67175364@N03");
    //f2.ListUsersAlbums("Norton Jack");
    //f2.ListUserAlbumPhotos("Jack Russell", "20231202 Marie");
    f2.FixFlickrFolder("D:/fd");
    return 0;
}