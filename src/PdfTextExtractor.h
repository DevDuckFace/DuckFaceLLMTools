#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <zlib.h>

// Best-effort PDF text extractor with no external dependencies beyond
// zlib (already pulled in transitively by curl). Handles the common case:
// text-based PDFs with FlateDecode-compressed content streams (the
// overwhelming majority of PDFs produced by Word, LaTeX, browsers, etc).
//
// What it CANNOT do: OCR scanned/image-only PDFs, or perfectly reconstruct
// layout/tables. For those, extraction will return little or no text —
// the caller should tell the user to paste text manually in that case.
namespace PdfTextExtractor {

inline std::vector<uint8_t> ReadFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

inline bool InflateBuffer(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    out.clear();
    if (in.empty()) return false;

    z_stream strm{};
    if (inflateInit(&strm) != Z_OK) return false;

    strm.next_in = const_cast<Bytef*>(in.data());
    strm.avail_in = (uInt)in.size();

    uint8_t buffer[8192];
    int ret;
    do {
        strm.next_out = buffer;
        strm.avail_out = sizeof(buffer);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            inflateEnd(&strm);
            return !out.empty();
        }
        size_t produced = sizeof(buffer) - strm.avail_out;
        out.insert(out.end(), buffer, buffer + produced);
    } while (ret != Z_STREAM_END && strm.avail_in > 0);

    inflateEnd(&strm);
    return true;
}

inline std::string ExtractTextFromContentStream(const std::string& stream) {
    std::string out;
    size_t i = 0;
    while (i < stream.size()) {
        if (stream[i] == '(') {
            std::string literal;
            size_t j = i + 1;
            int depth = 1;
            while (j < stream.size() && depth > 0) {
                char c = stream[j];
                if (c == '\\' && j + 1 < stream.size()) {
                    char next = stream[j + 1];
                    if (next == 'n') literal += '\n';
                    else if (next == 'r') literal += '\n';
                    else if (next == 't') literal += ' ';
                    else literal += next;
                    j += 2;
                    continue;
                }
                if (c == '(') depth++;
                else if (c == ')') { depth--; if (depth == 0) { j++; break; } }
                if (depth > 0) literal += c;
                j++;
            }
            out += literal;
            i = j;
        } else {
            i++;
        }
    }
    return out;
}

inline std::string Extract(const std::string& filePath, bool& outSuccess) {
    outSuccess = false;
    auto bytes = ReadFileBytes(filePath);
    if (bytes.empty()) return "";

    std::string raw(bytes.begin(), bytes.end());
    std::string result;

    const std::string streamTag = "stream";
    const std::string endStreamTag = "endstream";

    size_t pos = 0;
    while (true) {
        size_t streamPos = raw.find(streamTag, pos);
        if (streamPos == std::string::npos) break;

        size_t dataStart = streamPos + streamTag.size();
        if (dataStart < raw.size() && raw[dataStart] == '\r') dataStart++;
        if (dataStart < raw.size() && raw[dataStart] == '\n') dataStart++;

        size_t endPos = raw.find(endStreamTag, dataStart);
        if (endPos == std::string::npos) break;

        std::vector<uint8_t> compressed(bytes.begin() + dataStart, bytes.begin() + endPos);
        std::vector<uint8_t> decompressed;
        if (InflateBuffer(compressed, decompressed) && !decompressed.empty()) {
            std::string streamText(decompressed.begin(), decompressed.end());
            if (streamText.find("Tj") != std::string::npos || streamText.find("TJ") != std::string::npos) {
                std::string extracted = ExtractTextFromContentStream(streamText);
                if (!extracted.empty()) {
                    result += extracted;
                    result += "\n";
                }
            }
        }

        pos = endPos + endStreamTag.size();
    }

    outSuccess = !result.empty();
    return result;
}

} // namespace PdfTextExtractor
