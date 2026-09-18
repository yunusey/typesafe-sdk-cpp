#include "http.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <string>

namespace typesafe::detail
{

namespace
{

void ensure_global_init()
{
    static std::once_flag flag;
    std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

std::size_t write_body(char *ptr, std::size_t size, std::size_t nmemb, void *userdata)
{
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::size_t write_header(char *ptr, std::size_t size, std::size_t nmemb, void *userdata)
{
    auto *headers = static_cast<Headers *>(userdata);
    std::size_t total = size * nmemb;
    std::string line(ptr, total);

    // Strip trailing CRLF.
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
    {
        line.pop_back();
    }
    // Skip the status line and the blank separator line.
    auto colon = line.find(':');
    if (colon == std::string::npos)
    {
        return total;
    }
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    auto vbegin = std::find_if(value.begin(), value.end(), not_space);
    value.erase(value.begin(), vbegin);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    headers->append(std::move(name), std::move(value));
    return total;
}

struct SlistDeleter
{
    void operator()(curl_slist *list) const
    {
        if (list != nullptr)
        {
            curl_slist_free_all(list);
        }
    }
};

struct EasyDeleter
{
    void operator()(CURL *handle) const
    {
        if (handle != nullptr)
        {
            curl_easy_cleanup(handle);
        }
    }
};

} // namespace

Result<HttpResponse> http_execute(std::string_view method, const std::string &url, const Headers &headers,
                                  const std::optional<std::string> &body, Duration timeout)
{
    ensure_global_init();

    std::unique_ptr<CURL, EasyDeleter> handle(curl_easy_init());
    if (!handle)
    {
        return std::unexpected(Error::connection("failed to initialize HTTP client"));
    }
    CURL *curl = handle.get();

    std::string method_str(method);
    HttpResponse response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    long timeout_ms = static_cast<long>(timeout.count() * 1000.0);
    if (timeout_ms <= 0)
    {
        timeout_ms = 1;
    }
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

    if (body)
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->data());
    }

    std::unique_ptr<curl_slist, SlistDeleter> header_list;
    {
        curl_slist *raw = nullptr;
        for (const auto &[name, value] : headers)
        {
            std::string line = name;
            line += ": ";
            line += value;
            raw = curl_slist_append(raw, line.c_str());
        }
        header_list.reset(raw);
    }
    if (header_list)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
    }

    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
    {
        if (code == CURLE_OPERATION_TIMEDOUT)
        {
            return std::unexpected(Error::timeout(timeout));
        }
        return std::unexpected(Error::connection(curl_easy_strerror(code)));
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    response.status = static_cast<int>(status);
    return response;
}

} // namespace typesafe::detail
