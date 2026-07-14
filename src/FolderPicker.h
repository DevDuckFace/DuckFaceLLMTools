#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// Native Windows "Browse for Folder" dialog, used to pick the project
// folder the agent is allowed to work in. Returns an empty string if the
// user cancels.
namespace FolderPicker {

inline std::string OpenFolderDialog() {
    std::string result;

    HRESULT coInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needUninit = SUCCEEDED(coInit);

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(L"Select the project folder the agent can work in");

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        std::string buf(len, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, path, -1, buf.data(), len, nullptr, nullptr);
                        if (!buf.empty() && buf.back() == '\0') buf.pop_back();
                        result = buf;
                    }
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (needUninit) CoUninitialize();
    return result;
}

} // namespace FolderPicker
