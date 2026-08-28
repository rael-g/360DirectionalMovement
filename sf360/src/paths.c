#include "paths.h"

#include <windows.h>
#include <shlobj.h>
#include <wchar.h>

#include "sfse_version.h"

void sfse_folder(wchar_t *out, int capacity)
{
    PWSTR documents = NULL;
    if (!SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &documents))) {
        wcsncpy(out, L".", capacity);
        return;
    }
    swprintf(out, capacity, L"%ls\\My Games\\%hs\\SFSE", documents, SAVE_FOLDER_NAME);
    CoTaskMemFree(documents);
    CreateDirectoryW(out, NULL);
}

void log_path(wchar_t *out, int capacity)
{
    wchar_t folder[SF360_PATH_MAX];
    sfse_folder(folder, SF360_PATH_MAX);

    wchar_t logs[SF360_PATH_MAX];
    swprintf(logs, SF360_PATH_MAX, L"%ls\\Logs", folder);
    CreateDirectoryW(logs, NULL);

    swprintf(out, capacity, L"%ls\\sf360.log", logs);
}

void config_path(wchar_t *out, int capacity)
{
    wchar_t folder[SF360_PATH_MAX];
    sfse_folder(folder, SF360_PATH_MAX);
    swprintf(out, capacity, L"%ls\\sf360.ini", folder);
}
