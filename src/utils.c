#include "utils.h"

bool copy_to_clipboard(const char *str)
{
    if (!str)
        return 0;

    const size_t len = strlen(str) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hMem)
        return 0;

    memcpy(GlobalLock(hMem), str, len);
    GlobalUnlock(hMem);

    if (!OpenClipboard(NULL))
    {
        GlobalFree(hMem);
        return 0;
    }

    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();

    return 1;
}