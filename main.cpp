#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#include "flickrd2lib.h"

int main(/*int argc, wchar_t *argv[]*/)
{
    LF::Flickrd2 f2;
    f2.LoadCookies(LF::Browser_t::Firefox);
    return 0;
}