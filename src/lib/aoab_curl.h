#include <string>
#include <memory>
#include <iostream>
#include <string_view>
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

extern const std::string_view USERAGENT;

struct credentials;

class curl
{
public:
    curl() : _p(curl_easy_init()) {};

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

    template<typename T>
    int setopt(CURLoption option, T&& param) {
        auto code = curl_easy_setopt(_p.get(), option, std::forward<T>(param));
        if (CURLE_OK != code) [[unlikely]] {
            const char *estr = curl_easy_strerror(code);
            std::clog << "Error: Curl errorcode " << code << ": " << estr << '\n';
            return -1;
        }

        return 0;
    }

    void reset() {
        curl_easy_reset(_p.get());
    }

    CURLcode perform();

    void set_get_opts(std::ostream& write_stream, curlslistp& auth_header, std::string_view url);
    void set_post_opts(std::ostream& write_stream, std::istream &read_stream, std::string_view url);
    struct credentials login();

private:
    curlp _p;
};

struct credentials
{
    ~credentials()
    {
        if (auth_header) {
            logout();
        }
    }

    void logout()
    {
        c.reset();
        c.setopt(CURLOPT_POST, 1L);
        c.setopt(CURLOPT_POSTFIELDSIZE, 0L);
        c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
        c.setopt(CURLOPT_USERAGENT, USERAGENT.begin());
        c.setopt(CURLOPT_FAILONERROR, 1L);
        c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/auth/logout");
        auto code = c.perform();
        if (CURLE_OK != code ) {
            throw std::runtime_error("Failed to logout");
        }
    }

    curl &c;
    curlslistp auth_header;
};
