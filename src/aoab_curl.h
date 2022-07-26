#include <string>
#include <memory>
#include <iostream>
#include <curl/curl.h>

struct CURLEscapeDeleter {
    void operator()(char* str) const;
};

struct CURLEasyDeleter {
    void operator()(CURL* curl) const;
};

struct CURLShareDeleter {
    void operator()(CURLSH* share) const;
};

struct CURLSListDeleter {
    void operator()(struct curl_slist* list) const;
};

using curlp = std::unique_ptr<CURL, CURLEasyDeleter>;
using curlshp = std::unique_ptr<CURLSH, CURLShareDeleter>;
using curlstr = std::unique_ptr<char, CURLEscapeDeleter>;
using curlslistp = std::unique_ptr<struct curl_slist, CURLSListDeleter>;

class curl
{
public:
curl(curlp&& p) : _p(std::move(p)) {};
    ~curl() = default;

    template<typename T>
    int get_info(CURLINFO info, T&& val)
    {
        CURLcode code = curl_easy_getinfo(_p.get(), info, std::forward<T>(val));
        if (CURLE_OK != code) [[unlikely]] {
            const char *estr = curl_easy_strerror(code);
            std::clog << "Error: Curl errorcode " << code << ": " << estr << '\n';
            return -1;
        }

        return 0;
    }


    CURLcode curl_perform();

private:
    curlp _p;
};
