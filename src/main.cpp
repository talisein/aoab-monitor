#include <cassert>
#include <chrono>
#include <sstream>
#include <streambuf>
#include <ranges>
#include <locale>
#include "aoab_curl.h"
#include "defs.pb.h"

using namespace google::protobuf;

namespace {
    static constexpr auto USERAGENT {"aoab-monitor/1.0 (github/talisein/aoab-monitor)"};

    extern "C" {
        static size_t
        m_write(void *data, size_t size, size_t nmemb, void *userp)
        {
            auto s = static_cast<std::basic_ostream<char>*>(userp);
            std::streamsize count = size * nmemb;
            s->write(static_cast<char*>(data), count);
            return count;
        }

        static size_t
        m_read(char *ptr, size_t size, size_t nmemb, void *userdata)
        {
            auto ss = static_cast<std::basic_istream<char>*>(userdata);
            ss->read(ptr, size*nmemb);
            return ss->gcount();
        }

    }
}

// Returns Authorization header Bearer {access token}
curlslistp
do_login(curl &c)
{
    login_request l;
    std::stringstream ss_out, ss_in;
    const char *username = getenv("JNC_USERNAME");
    const char *password = getenv("JNC_PASSWORD");
    if (nullptr == username || nullptr == password) {
        std::cerr << "Environment variables JNC_USERNAME and JNC_PASSWORD must be set.\n";
        exit(1);
    }

    l.set_username(username);
    l.set_password(password);
    l.set_slim(true);
    auto serialized = l.SerializeToOstream(&ss_out);
    assert(serialized);

    c.setopt(CURLOPT_HSTS_CTRL, CURLHSTS_ENABLE);
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_POST, 1L);
    c.setopt(CURLOPT_READFUNCTION, m_read);
    c.setopt(CURLOPT_READDATA, &ss_out);
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &ss_in);

    curlslistp protobuf_header { curl_slist_append(nullptr, "Content-Type: application/vnd.google.protobuf") };
    c.setopt(CURLOPT_HTTPHEADER, protobuf_header.get());
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/auth/login");

    c.perform();

    login_response r;
    auto parsed = r.ParseFromIstream(&ss_in);
    assert(parsed);
    assert(r.token().size() > 0);
    std::stringstream ss;
    ss << "Authorization: Bearer " << r.token();
    auto header = ss.str();
    assert(header.size() > 0);
    return curlslistp{curl_slist_append(nullptr, header.c_str())};
}

struct data
{
    std::string slug;
    int64_t updated;
};

int main(int argc, char *argv[])
{
    std::locale::global(std::locale(""));
    curl_global_init(CURL_GLOBAL_SSL);

    curl c;

    // Login
    auto auth_header = do_login(c);

    // Fetch Library
    c.reset();
    c.setopt(CURLOPT_HSTS_CTRL, CURLHSTS_ENABLE);
    c.setopt(CURLOPT_HTTPGET, 1L);
    c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    std::stringstream ss;
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &ss);
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/me/library");
    c.perform();

    library_response r;
    auto parsed = r.ParseFromIstream(&ss);
    assert(parsed);

    // Print AOAB book updated times as JSON
    std::vector<data> v;
    for (const auto &book : r.books()) {
        if (auto& slug = book.volume().slug();
            slug.starts_with("ascendance") && book.has_lastupdated()) {
            v.emplace_back(slug, book.lastupdated().epoch_seconds());
        }
    }
    std::cout << "{\n";
    std::ranges::sort(v, std::ranges::greater{}, &data::updated);
    auto first = true;
    for (const auto &d : v) {
        if (first) {
            first = false;
        } else {
            std::cout << ",\n";
        }
        std::cout << "  \"" << d.slug << "\": " << d.updated;
    }
    std::cout << "\n}\n";

    // Logout
    c.reset();
    c.setopt(CURLOPT_HSTS_CTRL, CURLHSTS_ENABLE);
    c.setopt(CURLOPT_POST, 1L);
    c.setopt(CURLOPT_POSTFIELDSIZE, 0L);
    c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/auth/logout");
    c.perform();

    curl_global_cleanup();
    return 0;
}
