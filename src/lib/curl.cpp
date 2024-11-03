#include <sstream>
#include "aoab_curl.h"
#include "defs.pb.h"

void CURLEscapeDeleter::operator()(char* str) const
{
    curl_free(str);
}

void CURLEasyDeleter::operator()(CURL* curl) const
{
    curl_easy_cleanup(curl);
}

void CURLShareDeleter::operator()(CURLSH* share) const
{
    CURLSHcode code = curl_share_cleanup(share);
    if (code != CURLSHE_OK) [[unlikely]] {
        std::clog << "Failed to clean curl share: %s"
                  << curl_share_strerror(code) << '\n';
    }
}

void CURLSListDeleter::operator()(struct curl_slist* list) const
{
    curl_slist_free_all(list);
}

const std::string_view USERAGENT {"aoab-stats/1.0 (github/talisein/aoab-monitor)"};

namespace {

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

CURLcode curl::perform()
{
    CURLcode res = curl_easy_perform(_p.get());
    if (CURLE_OK != res) [[unlikely]] {
        const char *estr = curl_easy_strerror(res);
        std::clog << "Error performing http request ";

        long curl_errno {0};
        get_info(CURLINFO_OS_ERRNO, &curl_errno);
        char *url {nullptr};
        get_info(CURLINFO_EFFECTIVE_URL, &url);
        long response_code {0};
        get_info(CURLINFO_RESPONSE_CODE, &response_code);
        char *redirect_url {nullptr};
        get_info(CURLINFO_REDIRECT_URL, &redirect_url);
        curl_off_t uploaded {0};
        get_info(CURLINFO_SIZE_UPLOAD_T, &uploaded);
        curl_off_t downloaded {0};
        get_info(CURLINFO_SIZE_DOWNLOAD_T, &downloaded);
        long ssl_verify_result {0};
        get_info(CURLINFO_SSL_VERIFYRESULT, &ssl_verify_result);
        long num_connects {0};
        get_info(CURLINFO_NUM_CONNECTS, &num_connects);
        char *ip {nullptr};
        get_info(CURLINFO_PRIMARY_IP, &ip);
        long port {0};
        get_info(CURLINFO_PRIMARY_PORT, &port);
        double total_time {0.0};
        get_info(CURLINFO_TOTAL_TIME, &total_time);

        std::clog << "(" << response_code << "): " << estr << '\n';
        if (redirect_url) std::clog << "Redirect to: " << redirect_url << std::endl;
    } else {
        char *url {nullptr};
        get_info(CURLINFO_EFFECTIVE_URL, &url);
        long response_code {0};
        get_info(CURLINFO_RESPONSE_CODE, &response_code);
        char *redirect_url {nullptr};
        get_info(CURLINFO_REDIRECT_URL, &redirect_url);
        curl_off_t uploaded {0};
        get_info(CURLINFO_SIZE_UPLOAD_T, &uploaded);
        curl_off_t downloaded {0};
        get_info(CURLINFO_SIZE_DOWNLOAD_T, &downloaded);
        long ssl_verify_result {0};
        get_info(CURLINFO_SSL_VERIFYRESULT, &ssl_verify_result);
        long num_connects {0};
        get_info(CURLINFO_NUM_CONNECTS, &num_connects);
        char *ip {nullptr};
        get_info(CURLINFO_PRIMARY_IP, &ip);
        long port {0};
        get_info(CURLINFO_PRIMARY_PORT, &port);
        double total_time {0.0};
        get_info(CURLINFO_TOTAL_TIME, &total_time);
        std::clog << "\nCurl success. " << response_code << " " << url << " "
           << downloaded << "B down " << uploaded << "B up in "
           << total_time << "s\n";
        if (redirect_url) std::clog << "Redirect to: " << redirect_url << std::endl;
    }

    return res;
}

struct credentials
curl::login()
{
    jnovel::api::login_request l;
    std::stringstream ss_out, ss_in;
    const char *username = getenv("JNC_USERNAME");
    const char *password = getenv("JNC_PASSWORD");
    if (nullptr == username || nullptr == password || 0 == strlen(username) || 0 == strlen(password)) {
        std::cerr << "Environment variables JNC_USERNAME and JNC_PASSWORD must be set.\n";
        exit(EXIT_FAILURE);
    }

    l.set_username(username);
    l.set_password(password);
    l.set_slim(true);
    auto serialized = l.SerializeToOstream(&ss_out);
    if (!serialized) {
        throw std::runtime_error("Failed to serialize login_request");
    }

    setopt(CURLOPT_USERAGENT, std::to_address(USERAGENT.begin()));
    setopt(CURLOPT_POST, 1L);
    setopt(CURLOPT_READFUNCTION, m_read);
    setopt(CURLOPT_READDATA, &ss_out);
    setopt(CURLOPT_WRITEFUNCTION, m_write);
    setopt(CURLOPT_WRITEDATA, &ss_in);
    setopt(CURLOPT_FAILONERROR, 1L);

    curlslistp protobuf_header { curl_slist_append(nullptr, "Content-Type: application/vnd.google.protobuf") };
    setopt(CURLOPT_HTTPHEADER, protobuf_header.get());
    setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v2/auth/login");
    auto code = perform();
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to login");
    }

    jnovel::api::login_response r;
    auto parsed = r.ParseFromIstream(&ss_in);
    if (!parsed) {
        throw std::runtime_error("failed to parse login_response");
    }
    if (r.token().size() == 0) {
        throw std::runtime_error("login_response.token has size 0");
    }

    std::stringstream ss;
    ss << "Authorization: Bearer " << r.token();
    auto header = ss.str();
    return {
        .c = *this,
        .auth_header = curlslistp{curl_slist_append(nullptr, header.c_str())}
    };
}

void
curl::set_get_opts(std::ostream& write_stream, curlslistp& auth_header, std::string_view url)
{
    reset();
    setopt(CURLOPT_HTTPGET, 1L);
    setopt(CURLOPT_HTTPHEADER, auth_header.get());
    setopt(CURLOPT_USERAGENT, std::to_address(USERAGENT.begin()));
    setopt(CURLOPT_WRITEFUNCTION, m_write);
    setopt(CURLOPT_WRITEDATA, &write_stream);
    setopt(CURLOPT_FAILONERROR, 1L);
    setopt(CURLOPT_URL, std::to_address(url.begin()));
}

void
curl::set_post_opts(std::ostream& write_stream, std::istream &read_stream, std::string_view url)
{
    reset();
    setopt(CURLOPT_USERAGENT, std::to_address(USERAGENT.begin()));
    setopt(CURLOPT_POST, 1L);
    setopt(CURLOPT_READFUNCTION, m_read);
    setopt(CURLOPT_READDATA, &read_stream);
    setopt(CURLOPT_WRITEFUNCTION, m_write);
    setopt(CURLOPT_WRITEDATA, &write_stream);
    setopt(CURLOPT_FAILONERROR, 1L);

    curlslistp protobuf_header { curl_slist_append(nullptr, "Content-Type: application/vnd.google.protobuf") };
    setopt(CURLOPT_HTTPHEADER, protobuf_header.get());
    setopt(CURLOPT_URL, std::to_address(url.begin()));
}
