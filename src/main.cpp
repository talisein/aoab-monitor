#include <streambuf>
#include <locale>
#include <google/protobuf/dynamic_message.h>
#include "aoab_curl.h"
#include "defs.pb.h"

using namespace google::protobuf;

struct mybuf : public std::streambuf
{
    mybuf() { v.reserve(33729); };

    virtual std::streamsize xsputn( const char_type* s, std::streamsize count ) override {
        v.insert(v.end(), s, s + count);
        return count;
    }

    std::vector<char> v;
};

namespace {

    extern "C" {
        static size_t
        m_write(void *data, size_t size, size_t nmemb, void *userp)
        {
            auto s = static_cast<std::string*>(userp);
            s->append(static_cast<char*>(data), size * nmemb);
            return size*nmemb;
        }
    }
} // namespace

int main(int argc, char *argv[])
{
    std::locale::global(std::locale(""));
    curl_global_init(CURL_GLOBAL_SSL);

    std::string s;
    curl c;

    c.setopt(CURLOPT_HSTS_CTRL, CURLHSTS_ENABLE);
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/series/5c8df1ef2f5f17684dd287b6/volumes");
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &s);
    c.perform();

    volumes_response v;
    if (!v.ParseFromString(s)) {
        std::cerr << "parse failure\n";
    }

    for (const auto &vol : v.volumes()) {
        std::cout << vol.slug() << "\n";
    }

    curl_global_cleanup();
    return 0;
}
