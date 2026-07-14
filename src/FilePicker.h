#pragma once
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <cctype>
#include <cstring>

#pragma comment(lib, "comdlg32.lib")

// Native Windows "Open File" dialog, used to attach an image or PDF to a
// chat message. Returns an empty string if the user cancels.
namespace FilePicker {

inline std::string OpenImageOrPdfDialog() {
    char fileBuf[MAX_PATH] = "";

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter =
        "Images and PDFs\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.pdf\0"
        "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0"
        "PDF Documents\0*.pdf\0"
        "All Files\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Attach image or PDF";

    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileBuf);
    }
    return "";
}

inline bool HasExtension(const std::string& path, const char* ext) {
    if (path.size() < strlen(ext)) return false;
    std::string tail = path.substr(path.size() - strlen(ext));
    for (auto& c : tail) c = (char)tolower((unsigned char)c);
    std::string extLower = ext;
    for (auto& c : extLower) c = (char)tolower((unsigned char)c);
    return tail == extLower;
}

inline bool IsImage(const std::string& path) {
    return HasExtension(path, ".png") || HasExtension(path, ".jpg") ||
           HasExtension(path, ".jpeg") || HasExtension(path, ".bmp") ||
           HasExtension(path, ".gif");
}

inline bool IsPdf(const std::string& path) {
    return HasExtension(path, ".pdf");
}

inline std::string MimeTypeFor(const std::string& path) {
    if (HasExtension(path, ".png")) return "image/png";
    if (HasExtension(path, ".jpg") || HasExtension(path, ".jpeg")) return "image/jpeg";
    if (HasExtension(path, ".bmp")) return "image/bmp";
    if (HasExtension(path, ".gif")) return "image/gif";
    return "application/octet-stream";
}

inline std::string OpenGgufDialog(const char* title) {
    char fileBuf[MAX_PATH] = "";

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "GGUF model files\0*.gguf\0All Files\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = title;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileBuf);
    }
    return "";
}

} // namespace FilePicker
