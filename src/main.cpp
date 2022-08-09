#include <experimental/iterator>
#include <iomanip>
#include <chrono>
#include <optional>
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
        c.setopt(CURLOPT_USERAGENT, USERAGENT);
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

// Returns Authorization header Bearer {access token}
static credentials
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
    if (!serialized) {
        throw std::runtime_error("Failed to serialize login_request");
    }

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
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to login");
    }

    login_response r;
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
        .c = c,
        .auth_header = curlslistp{curl_slist_append(nullptr, header.c_str())}
    };
}

static std::vector<book>
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
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to fetch library_response");
    }

    library_response res;
    auto parsed = res.ParseFromIstream(&ss);
    if (!parsed) {
        throw std::runtime_error("Failed to parse library_response");
    }
    if (!res.pagination().lastpage()) {
        std::cerr << "Fetched library, but this isn't the last page! Need to implement multiple calls.\n";
    }

    return {std::ranges::begin(res.books()), std::ranges::end(res.books())};
}

static void print_json(std::vector<book> &books)
{
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
    //   os << date::format("%e %B %Y-W%V-%u", sec);
    os << date::format("%e %B %Y", sec);
}

static void print_human(std::vector<book> &books)
{
    using namespace std::string_view_literals;
    // sort by lastUpdated
    std::ranges::sort(books, std::ranges::greater{},
                      [](const auto &book) { return book.volume().publishing().epoch_seconds(); });

    // Filter out everything except AOAB
    auto filter = std::ranges::views::filter(
        [](const auto &book) {
            return book.volume().slug().starts_with("ascendance"sv)
                && book.volume().has_publishing();
        });

    auto not_manga_filter = std::ranges::views::filter(
        [](const auto &book) {
            return std::string::npos == book.volume().title().find("Manga"sv);
        });
    auto manga_filter = std::ranges::views::filter(
        [](const auto &book) {
            return std::string::npos != book.volume().title().find("Manga"sv);
        });
    auto is_part_filter = std::ranges::views::filter(
        [](const auto &book) {
            return std::string::npos != book.volume().title().find("Part"sv);
        });
    auto is_not_part_filter = std::ranges::views::filter(
        [](const auto &book) {
            return std::string::npos == book.volume().title().find("Part"sv);
        });

    using interpart_durations_t = std::map<std::string, std::optional<std::chrono::days>>;
    interpart_durations_t interpart_durations;
    std::optional<date::sys_seconds> prev;
    auto interpart_xform = std::ranges::views::transform(
        [&prev](const auto &book) -> interpart_durations_t::value_type {
            auto slug = book.volume().slug();
            date::sys_seconds cur = date::sys_seconds(std::chrono::seconds(book.volume().publishing().epoch_seconds()));
            std::optional<std::chrono::days> diff;
            if (prev) {
                diff = std::chrono::duration_cast<std::chrono::days>(cur - *prev);
            }
            prev = cur;
            return std::make_pair(slug, diff);
        });
    std::ranges::copy(std::ranges::reverse_view(books) | filter | not_manga_filter | is_part_filter | interpart_xform,
                      std::inserter(interpart_durations, interpart_durations.end()));
    prev = std::nullopt;
    std::ranges::copy(std::ranges::reverse_view(books) | filter | not_manga_filter | is_not_part_filter | interpart_xform,
                      std::inserter(interpart_durations, interpart_durations.end()));
    prev = std::nullopt;
    std::ranges::copy(std::ranges::reverse_view(books) | filter | manga_filter | is_part_filter | interpart_xform,
                      std::inserter(interpart_durations, interpart_durations.end()));

    // Format html
    auto transform = std::ranges::views::transform(
        [&interpart_durations](const auto& book) -> std::string {
            std::stringstream ss;
            ss << "\t<tr>\n";
            ss << "\t\t<td>" << book.volume().title() << "</td>\n";
            ss << "\t\t<td class=\"righted\">";
            ts_to_ostream(ss, book.volume().publishing());
            ss << "</td>\n";
            ss << "\t\t<td class=\"centered\">";
            if (auto iter = interpart_durations.find(book.volume().slug()); iter != interpart_durations.end()) {
                if (iter->second) {
                    ss << iter->second->count();
                }
            }
            ss << "</td>\n";

            if (book.has_lastupdated()) {
                ss << "\t\t<td class=\"righted\">";
                if (book.lastupdated().epoch_seconds() > book.volume().publishing().epoch_seconds()) {
                    ts_to_ostream(ss, book.lastupdated());
                }
                ss << "</td>\n";
            }
            ss << "\t</tr>\n";
            return ss.str();
            });


    std::ofstream out("index.html");

    out << R"html(<!DOCTYPE html>
<html>
<style>

td, th {
  padding: 8px;
}

.centered {
   text-align: center;
}

.righted {
   text-align: right;
}

tr:nth-child(even) {background-color: #f2f2f2;}
</style>
<body>

<h2>Ascendence of a Bookworm Releases</h2>

<table>
  <tr>
    <th>Title</th>
    <th>Published</th>
    <th>Days</th>
    <th>Updated</th>
  </tr>
)html";
    auto ln_view = transform(books | filter | not_manga_filter);
    std::copy(ln_view.begin(), ln_view.end(),
              std::ostream_iterator<std::string>(out));
    out << R"html(</table>
<h2>Ascendence of a Bookworm Manga Releases</h2>

<table>
  <tr>
    <th>Title</th>
    <th>Published</th>
    <th>Days</th>
)html";

    if (std::ranges::any_of(books | filter | manga_filter, &book::has_lastupdated)) {
        out << R"html(<th>Updated</th>
  </tr>
)html";
    }


    auto manga_view = transform(books | filter | manga_filter);
    std::copy(manga_view.begin(), manga_view.end(),
              std::ostream_iterator<std::string>(out));
    out << R"html(</table>
</body>
</html>
)html";
}


int main(int, char *[])
{
    curl_global_init(CURL_GLOBAL_SSL);

    try {
        curl c;
        auto cred = login(c);
        auto books = fetch_library(c, cred.auth_header);
        print_json(books);
        print_human(books);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    curl_global_cleanup();
    return EXIT_SUCCESS;
}
