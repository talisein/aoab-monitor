#include <experimental/iterator>
#include <cassert>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <ranges>
#include <locale>
#include "date/date.h"
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
static curlslistp
login(curl &c)
{
    login_request l;
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
    assert(serialized);

    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_POST, 1L);
    c.setopt(CURLOPT_READFUNCTION, m_read);
    c.setopt(CURLOPT_READDATA, &ss_out);
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &ss_in);
    c.setopt(CURLOPT_FAILONERROR, 1L);

    curlslistp protobuf_header { curl_slist_append(nullptr, "Content-Type: application/vnd.google.protobuf") };
    c.setopt(CURLOPT_HTTPHEADER, protobuf_header.get());
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/auth/login");
    auto code = c.perform();
    assert(CURLE_OK == code);

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

static library_response
fetch_library(curl& c, curlslistp& auth_header)
{
    std::stringstream ss;

    c.reset();
    c.setopt(CURLOPT_HTTPGET, 1L);
    c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &ss);
    c.setopt(CURLOPT_FAILONERROR, 1L);
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/me/library");
    auto code = c.perform();
    assert(CURLE_OK == code);

    library_response res;
    auto parsed = res.ParseFromIstream(&ss);
    assert(parsed);
    return res;
}

static void print_json(library_response &r)
{
    // Have to copy books to sort
    std::vector<book> books(r.books().begin(), r.books().end());
    // sort by lastUpdated
    std::ranges::sort(books, std::ranges::greater{},
                      [](const auto &book) { return book.lastupdated().epoch_seconds(); });

    // Filter out everything except AOAB
    auto filter = std::ranges::views::filter(
        [](const auto &book) {
            return book.volume().slug().starts_with("ascendance")
                && book.has_lastupdated();
        });
    // Format JSON string
    auto transform = std::ranges::views::transform(
        [](const auto& book) -> std::string {
            std::stringstream ss;
            ss << "  " << std::quoted(book.volume().slug()) << ": "
               << book.lastupdated().epoch_seconds();
            return ss.str();
            });

    // Print
    std::cout << "{\n";
    auto view = transform(books | filter);
    std::copy(view.begin(), view.end(),
              std::experimental::make_ostream_joiner(std::cout, ",\n"));
    std::cout << "\n}\n";
}

static void ts_to_ostream(std::ostream &os, const timestamp& ts)
{
    date::sys_seconds sec(std::chrono::seconds(ts.epoch_seconds()));
    os << date::format("%c", sec);
}

static void print_human(const library_response &r)
{
    // Have to copy books to sort
    std::vector<book> books(r.books().begin(), r.books().end());
    // sort by lastUpdated
    std::ranges::sort(books, std::ranges::greater{},
                      [](const auto &book) { return book.volume().publishing().epoch_seconds(); });

    // Filter out everything except AOAB
    auto filter = std::ranges::views::filter(
        [](const auto &book) {
            return book.volume().slug().starts_with("ascendance")
                && book.volume().has_publishing();
        });
    // Format html
    auto transform = std::ranges::views::transform(
        [](const auto& book) -> std::string {
            std::stringstream ss;
            ss << "\t<tr>\n";
            ss << "\t\t<td>" << book.volume().title() << "</td>\n";
            ss << "\t\t<td>";
            ts_to_ostream(ss, book.volume().publishing());
            ss << "</td>\n";
            ss << "\t\t<td>";
            if (book.has_lastupdated()) {
                ts_to_ostream(ss, book.lastupdated());
            }
            ss << "</td>\n";
            ss << "\t</tr>\n";
            return ss.str();
            });


    std::ofstream out("latest.html");

    out << R"html(<!DOCTYPE html>
<html>
<style>
table, th, td {
  border:1px solid black;
}
</style>
<body>

<h2>Ascendence of a Bookworm Releases</h2>

<table>
  <tr>
    <th>Title</th>
    <th>Published</th>
    <th>Updated</th>
  </tr>
)html";
    auto view = transform(books | filter);
    std::copy(view.begin(), view.end(),
              std::ostream_iterator<std::string>(out));
    out << R"html(</table>
</body>
</html>
)html";
}


static void
logout(curl &c, curlslistp& auth_header)
{
    c.reset();
    c.setopt(CURLOPT_POST, 1L);
    c.setopt(CURLOPT_POSTFIELDSIZE, 0L);
    c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_FAILONERROR, 1L);
    c.setopt(CURLOPT_URL, "https://labs.j-novel.club/app/v1/auth/logout");
    auto code = c.perform();
    assert(CURLE_OK == code);
}

int main(int, char *[])
{
    curl_global_init(CURL_GLOBAL_SSL);
    curl c;

    auto auth_header = login(c);
    library_response r = fetch_library(c, auth_header);
    print_json(r);
    print_human(r);
    logout(c, auth_header);

    curl_global_cleanup();
    return EXIT_SUCCESS;
}
