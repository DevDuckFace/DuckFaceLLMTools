#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <curl/curl.h>

// Busca na web via DuckDuckGo HTML (html.duckduckgo.com) — nao requer
// chave de API, cadastro ou custo. Fluxo: busca -> pega os N primeiros
// resultados -> baixa cada pagina -> extrai o texto (removendo tags,
// scripts e estilos) -> devolve tudo pronto para injetar no contexto do
// modelo. Limitacoes conhecidas: paginas que dependem de JavaScript ou
// paywall rendem pouco texto util.
namespace WebSearch {

struct Result {
    std::string title;
    std::string url;
    std::string snippet;
    std::string pageText; // texto extraido da pagina (pode ficar vazio se o fetch falhar)
};

inline size_t WriteCb(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buf = static_cast<std::string*>(userp);
    // Protege contra paginas gigantes: 2 MB de HTML e mais que suficiente
    // para extrair texto util e evita estourar memoria/tempo.
    if (buf->size() > 2 * 1024 * 1024) return 0;
    buf->append((char*)contents, size * nmemb);
    return size * nmemb;
}

inline std::string HttpGet(const std::string& url, long timeoutSec = 12) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // aceita gzip/deflate
    // User-Agent de navegador comum: varios sites bloqueiam UAs de bot.
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36");
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK ? body : "";
}

inline std::string UrlEncode(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out << c;
        else if (c == ' ') out << '+';
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); out << b; }
    }
    return out.str();
}

inline std::string UrlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else if (s[i] == '+') out += ' ';
        else out += s[i];
    }
    return out;
}

inline std::string DecodeHtmlEntities(std::string s) {
    struct { const char* from; const char* to; } ents[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&#x27;", "'"}, {"&#39;", "'"}, {"&nbsp;", " "}, {"&mdash;", "-"},
        {"&ndash;", "-"}, {"&hellip;", "..."},
    };
    for (auto& e : ents) {
        size_t p = 0;
        while ((p = s.find(e.from, p)) != std::string::npos) {
            s.replace(p, strlen(e.from), e.to);
            p += strlen(e.to);
        }
    }
    return s;
}

// Remove um bloco <tag ...>...</tag> inteiro (script, style, nav...).
inline void StripBlocks(std::string& html, const std::string& tag) {
    std::string open = "<" + tag, close = "</" + tag + ">";
    size_t p = 0;
    while ((p = html.find(open, p)) != std::string::npos) {
        size_t end = html.find(close, p);
        if (end == std::string::npos) { html.erase(p); break; }
        html.erase(p, end - p + close.size());
    }
}

// Converte HTML em texto plano legivel (o suficiente para o modelo — nao
// precisa ser perfeito, precisa ser informativo).
inline std::string HtmlToText(std::string html, size_t maxChars = 6000) {
    StripBlocks(html, "script");
    StripBlocks(html, "style");
    StripBlocks(html, "noscript");
    StripBlocks(html, "svg");
    StripBlocks(html, "header");
    StripBlocks(html, "footer");
    StripBlocks(html, "nav");

    std::string out;
    out.reserve(html.size() / 4);
    bool inTag = false;
    for (size_t i = 0; i < html.size(); i++) {
        char c = html[i];
        if (c == '<') {
            inTag = true;
            // quebras de linha para tags de bloco comuns
            if (html.compare(i, 3, "<p ") == 0 || html.compare(i, 3, "<p>") == 0 ||
                html.compare(i, 4, "<br") == 0 || html.compare(i, 3, "<li") == 0 ||
                html.compare(i, 3, "<h1") == 0 || html.compare(i, 3, "<h2") == 0 ||
                html.compare(i, 3, "<h3") == 0 || html.compare(i, 4, "<div") == 0 ||
                html.compare(i, 3, "<tr") == 0)
                out += '\n';
        } else if (c == '>') inTag = false;
        else if (!inTag) out += c;
    }
    out = DecodeHtmlEntities(out);

    // Colapsa espacos/linhas em excesso
    std::string clean;
    clean.reserve(out.size());
    int nl = 0, sp = 0;
    for (char c : out) {
        if (c == '\r') continue;
        if (c == '\n') { nl++; sp = 0; if (nl <= 2) clean += c; continue; }
        if (c == ' ' || c == '\t') { sp++; if (sp <= 1 && !clean.empty() && clean.back() != '\n') clean += ' '; continue; }
        nl = 0; sp = 0;
        clean += c;
    }
    if (clean.size() > maxChars) {
        clean.resize(maxChars);
        clean += "\n[...pagina truncada...]";
    }
    return clean;
}

// Faz a busca no DuckDuckGo e devolve ate maxResults resultados. Os links
// vem embrulhados num redirect (//duckduckgo.com/l/?uddg=<url-encodada>);
// extraimos a URL real do parametro uddg.
inline std::vector<Result> Search(const std::string& query, int maxResults = 3) {
    std::vector<Result> results;
    std::string html = HttpGet("https://html.duckduckgo.com/html/?q=" + UrlEncode(query));
    if (html.empty()) return results;

    size_t pos = 0;
    while (results.size() < (size_t)maxResults) {
        size_t a = html.find("class=\"result__a\"", pos);
        if (a == std::string::npos) break;
        size_t tagStart = html.rfind("<a ", a);
        if (tagStart == std::string::npos) break;

        // href
        size_t h1 = html.find("href=\"", tagStart);
        size_t h2 = html.find("\"", h1 + 6);
        if (h1 == std::string::npos || h2 == std::string::npos) break;
        std::string href = html.substr(h1 + 6, h2 - h1 - 6);

        // titulo = conteudo do <a>
        size_t t1 = html.find(">", h2);
        size_t t2 = html.find("</a>", t1);
        if (t1 == std::string::npos || t2 == std::string::npos) break;
        std::string title = HtmlToText(html.substr(t1 + 1, t2 - t1 - 1), 300);

        // snippet do resultado (se existir)
        std::string snippet;
        size_t s1 = html.find("result__snippet", t2);
        size_t nextResult = html.find("class=\"result__a\"", t2);
        if (s1 != std::string::npos && (nextResult == std::string::npos || s1 < nextResult)) {
            size_t sTag = html.find(">", s1);
            size_t sEnd = html.find("</a>", sTag);
            if (sEnd == std::string::npos) sEnd = html.find("</div>", sTag);
            if (sTag != std::string::npos && sEnd != std::string::npos)
                snippet = HtmlToText(html.substr(sTag + 1, sEnd - sTag - 1), 500);
        }

        // resolve o redirect do DDG para a URL real
        std::string url = href;
        size_t uddg = href.find("uddg=");
        if (uddg != std::string::npos) {
            std::string enc = href.substr(uddg + 5);
            size_t amp = enc.find('&');
            if (amp != std::string::npos) enc = enc.substr(0, amp);
            url = UrlDecode(enc);
        }
        if (url.rfind("//", 0) == 0) url = "https:" + url;

        // ignora anuncios/links internos do DDG
        if (url.find("duckduckgo.com") == std::string::npos && url.rfind("http", 0) == 0) {
            Result r;
            r.title = title;
            r.url = url;
            r.snippet = snippet;
            results.push_back(std::move(r));
        }
        pos = t2;
    }
    return results;
}

// Pipeline completo: busca + download das paginas + montagem do bloco de
// contexto pronto para injetar no prompt. `sourcesOut` recebe as URLs
// usadas, para citar no fim da resposta.
inline std::string BuildContext(const std::string& query,
                                 std::vector<std::string>& sourcesOut,
                                 int maxResults = 3,
                                 size_t maxCharsPerPage = 6000) {
    auto results = Search(query, maxResults);
    if (results.empty()) return "";

    std::ostringstream ctx;
    ctx << "[WEB SEARCH RESULTS - retrieved just now from the internet. "
        << "Use this up-to-date information to answer the user's question. "
        << "If the results don't contain the answer, say so honestly.]\n\n";

    int n = 1;
    for (auto& r : results) {
        r.pageText = HtmlToText(HttpGet(r.url), maxCharsPerPage);
        ctx << "=== Source " << n << ": " << r.title << " (" << r.url << ") ===\n";
        if (!r.snippet.empty()) ctx << "Summary: " << r.snippet << "\n";
        if (!r.pageText.empty()) ctx << r.pageText << "\n";
        ctx << "\n";
        sourcesOut.push_back(r.url);
        n++;
    }
    return ctx.str();
}

} // namespace WebSearch
