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
#include "utils.h"
#include "date/tz.h"
using namespace google::protobuf;

static std::vector<jnovel::api::book>
fetch_library(curl& c, curlslistp& auth_header)
{
    std::stringstream ss;

    c.set_get_opts(ss, auth_header, "https://labs.j-novel.club/app/v1/me/library");
    auto code = c.perform();
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to fetch library_response");
    }

    jnovel::api::library_response res;
    auto parsed = res.ParseFromIstream(&ss);
    if (!parsed) {
        throw std::runtime_error("Failed to parse library_response");
    }
    if (!res.pagination().lastpage()) {
        std::cerr << "Fetched library, but this isn't the last page! Need to implement multiple calls.\n";
    }

    return {std::ranges::begin(res.books()), std::ranges::end(res.books())};
}

static void print_json(std::vector<jnovel::api::book> &books)
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

static void ts_to_ostream(std::ostream &os, const jnovel::api::timestamp& ts)
{
    date::sys_seconds sec(std::chrono::seconds(ts.epoch_seconds()));
    //   os << date::format("%e %B %Y-W%V-%u", sec);
    os << date::format("%e %B %Y", sec);
}

void write_next(int vol, auto ts) {
    std::ofstream out("next.html");
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

<h2>Ascendence of a Bookworm Predicted Release Dates</h2>

<table>
  <tr>
    <th>Title</th>
    <th>Stream Start Prediction</th>
    <th>Publish Prediction</th>
  </tr>
)html";

    auto weeks = std::chrono::weeks(8);
    using namespace std::chrono_literals;
    constexpr auto pub_date_1 = std::chrono::sys_days{std::chrono::May/31/2023};
    constexpr auto pub_date_2 = std::chrono::sys_days{std::chrono::March/13/2023};
    constexpr auto pub_lag = pub_date_1 - pub_date_2;

    const std::chrono::sys_days rootts = std::chrono::round<std::chrono::days>(std::chrono::sys_seconds(std::chrono::seconds(ts.epoch_seconds())));
    for (int i = vol+1; i <= 12; ++i, weeks += std::chrono::weeks(8)) {
        std::chrono::sys_days nextts {rootts + weeks};
        std::stringstream ss;
        ss << "\t<tr>\n";
        ss << "\t\t<td>" << "Part 5 Volume " << i << "</td>\n";
        ss << "\t\t<td class=\"righted\">";

        std::chrono::sys_days streaming_date_close {nextts - pub_lag};
        date::year_month_day streaming_date {streaming_date_close};
        /* Find closest Monday */
        for (std::chrono::days i = std::chrono::days(0); i < std::chrono::days(5); ++i) {
            if ((std::chrono::weekday(streaming_date_close) + i) == std::chrono::Monday) {
                streaming_date = date::year_month_day{streaming_date_close + i};
                break;
            }
            if ((std::chrono::weekday(streaming_date_close) - i) == std::chrono::Monday) {
                streaming_date = date::year_month_day{streaming_date_close - i};
                break;
            }
        }
        ss << date::format("%e %B %Y", streaming_date);
        ss << "</td>\n";
        ss << "\t\t<td class=\"righted\">";
        ss << date::format("%e %B %Y", nextts);
        ss << "</td>\n";

        ss << "\t</tr>\n";
        out << ss.str();
    }

    out << R"html(</table>
</body>
</html>
)html";

}

static void print_human(std::vector<jnovel::api::book> &books)
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
    auto is_part_5_filter = std::ranges::views::filter(
        [](const auto &book) {
            return std::string::npos != book.volume().title().find("Part 5"sv);
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
<p><a href="../">top</a></p>
<h2>Ascendence of a Bookworm Releases</h2>
<p><a href="stats">Weekly Statistics</a></p>
<p><a href="next.html">Future Volume Release Day Estimates</a></p>
<p>Discord Timestamp for next release &lt;t:)html";
    using namespace std::chrono_literals;
    auto zone = date::locate_zone("America/Los_Angeles");
    const date::zoned_time zt{zone, std::chrono::system_clock::now()};
    auto now = zt.get_local_time();
    auto now_days = date::floor<date::days>(now);
    auto now_weekday = date::weekday(now_days);
    date::local_days next_monday;
    if (now_weekday == date::Tuesday) {
        next_monday = now_days + date::days(6);
    } else if (now_weekday == date::Wednesday) {
        next_monday = now_days + date::days(5);
    } else if (now_weekday == date::Thursday) {
        next_monday = now_days + date::days(4);
    } else if (now_weekday == date::Friday) {
        next_monday = now_days + date::days(3);
    } else if (now_weekday == date::Saturday) {
        next_monday = now_days + date::days(2);
    } else if (now_weekday == date::Sunday) {
        next_monday = now_days + date::days(1);
    } else if (now_weekday == date::Monday) {
        date::zoned_time threshold{zone, now_days + 14h};
        if (now < threshold.get_local_time()) {
            next_monday = now_days;
        } else {
            next_monday = now_days + date::days(7);
        }
    }

    auto next_drop = date::zoned_time{zone, next_monday + 14h};
    date::sys_seconds zero_utc{};
    out << (next_drop.get_sys_time() - zero_utc).count();

    out << R"html(:f&gt;</p>
)html";
    out << R"html(<table>
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

    if (std::ranges::any_of(books | filter | manga_filter, &jnovel::api::book::has_lastupdated)) {
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


    auto part_view = books | is_part_5_filter;
    auto last_part = std::ranges::max(part_view, std::ranges::less{}, [](const auto& x) { return slug_to_volume_number(x.volume().slug()); });
    write_next(slug_to_volume_number(last_part.volume().slug()),
               last_part.volume().publishing());
}


int main(int, char *[])
{
    curl_global_init(CURL_GLOBAL_SSL);

    try {
        curl c;
        auto cred = c.login();
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
