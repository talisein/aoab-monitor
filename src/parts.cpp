#include <experimental/iterator>
#include <iomanip>
#include <chrono>
#include <optional>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <ranges>
#include <charconv>
#include <streambuf>
#include <ranges>
#include <locale>
#include <system_error>
#include <libxml++/libxml++.h>
#include "date/date.h"
#include "aoab_curl.h"
#include "defs.pb.h"

using namespace google::protobuf;

namespace {
    static constexpr auto USERAGENT {"aoab-stats/1.0 (github/talisein/aoab-monitor)"};

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

using  id_map_t = std::map<std::string, std::string>;
id_map_t
fetch_partlist(curl& c, curlslistp& auth_header, std::string_view volume_slug)
{
    std::stringstream url_ss;
    url_ss << "https://labs.j-novel.club/app/v1/parts/" << volume_slug << "/toc";
    auto url = url_ss.str();

    std::stringstream ss;

    c.reset();
    c.setopt(CURLOPT_HTTPGET, 1L);
    c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &ss);
    c.setopt(CURLOPT_FAILONERROR, 1L);
    c.setopt(CURLOPT_URL, url.c_str());

//    curlslistp json_header { curl_slist_append(nullptr, "Content-Type: application/json") };
//    c.setopt(CURLOPT_HTTPHEADER, json_header.get());

    auto code = c.perform();
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to fetch library_response");
    }

    toc_response r;
    auto parsed = r.ParseFromIstream(&ss);
    if (!parsed) {
        throw std::runtime_error("failed to parse parts_response");
    }

    id_map_t res;
    for (auto &part : r.parts().parts()) {
        res.try_emplace(part.legacyid(), part.slug());
    }

    return res;
}

void
fetch_part(curl& c, curlslistp& auth_header, const id_map_t::value_type& id, std::stringstream &ofs)
{
    std::stringstream url_ss;
    url_ss << "https://labs.j-novel.club/embed/" << id.first << "/data.xhtml";
    auto url = url_ss.str();

    c.reset();
    c.setopt(CURLOPT_HTTPGET, 1L);
    c.setopt(CURLOPT_HTTPHEADER, auth_header.get());
    c.setopt(CURLOPT_USERAGENT, USERAGENT);
    c.setopt(CURLOPT_WRITEFUNCTION, m_write);
    c.setopt(CURLOPT_WRITEDATA, &ofs);
    c.setopt(CURLOPT_FAILONERROR, 1L);
    c.setopt(CURLOPT_URL, url.c_str());

    auto code = c.perform();
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to fetch library_response");
    }
}

using wordstat_map_t = std::map<id_map_t::value_type, int>;
wordstat_map_t
read_wordstat_file(std::filesystem::path filename)
{
    wordstat_map_t res;

    std::fstream wordstats_is(filename, wordstats_is.in);
    if (!wordstats_is.is_open()) {
        try {
            wordstats_is.exceptions(wordstats_is.failbit | wordstats_is.badbit);
        } catch (std::ios_base::failure &e) {
            throw std::system_error(e.code(), "Can't open wordstat file");
        } catch (...) { throw; }
    }

    for (std::string line; std::getline(wordstats_is, line); ) {
        // 12483,6384ed590517e1e171e6c551,ascendance-of-a-bookworm-part-5-volume-2-part-4
        constexpr std::string_view delim{","};
        auto r = std::views::split(line, delim);
        auto it = r.begin();
        int words;
        auto fromchars_res = std::from_chars(std::to_address(std::ranges::begin(*it)),
                                             std::to_address(std::ranges::end(*it)),
                                             words);
        if (fromchars_res.ec != std::errc()) {
            throw std::system_error(std::make_error_code(fromchars_res.ec), "Couldn't parse words int from csv");
        }
        ++it; if (it == std::ranges::end(r)) throw std::system_error(std::make_error_code(std::errc::invalid_seek), "csv has less than 2 elements");
        auto legacy_id = std::string_view(std::ranges::begin(*it), std::ranges::end(*it));
        ++it; if (it == std::ranges::end(r)) throw std::system_error(std::make_error_code(std::errc::invalid_seek), "csv has less than 3 elements");
        auto slug = std::string_view(std::ranges::begin(*it), std::ranges::end(*it));

        res.try_emplace(id_map_t::value_type(legacy_id, slug), words);
    }

    return res;
}

void
write_wordstat_file(std::filesystem::path filename, const wordstat_map_t& stats)
{
    std::fstream wordstats_os(filename, wordstats_os.out | wordstats_os.trunc);
    if (!wordstats_os.is_open()) {
        try {
            wordstats_os.exceptions(wordstats_os.failbit | wordstats_os.badbit);
        } catch (std::ios_base::failure &e) {
            throw std::system_error(e.code(), "Can't open wordstat file for writing");
        } catch (...) { throw; }
        return;
    }

    for (const auto &stat : stats) {
        wordstats_os << stat.second << ',' << stat.first.first << ',' << stat.first.second << '\n';
    }

    wordstats_os.close();
}

class word_parser : public xmlpp::SaxParser
{
    public:
    word_parser() {};
    ~word_parser() override {};

    int count() {
        std::ranges::istream_view<std::string> v(ss);

        return std::ranges::distance(v);
    }
protected:
    void on_characters(const xmlpp::ustring& characters) override {
        ss << characters;
    };

private:
    std::stringstream ss;
};


int
count_words(std::stringstream& ss)
{
    word_parser p;
    p.parse_stream(ss);
    return p.count();
}


constexpr std::string_view TEST_DATA = R"html(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xmlns:xml="http://www.w3.org/XML/1998/namespace" lang="en" xml:lang="en">
<head>
  <meta content="text/html; charset=UTF-8" http-equiv="content-type"/>
  <title>Ascendance of a Bookworm: Part 4 Volume 3 Part 8</title>
  <link href="https://m11.j-novel.club/style/novel-rough.css" rel="stylesheet" type="text/css"/>
</head>
<body>
  <section epub:type="bodymatter chapter">
    <!--I would not recommend using this content to build EPUBs-->
    <div class="main">
      <h1>Epilogue</h1>
      <p>After her request for praise, Rozemyne gave a half-hearted smile and lowered her gaze. It was the expression she made when giving up on something—such as when she had given up on visiting the library in the Royal Academy, or when she had conceded that her separation from those in the lower city was necessary. But what had she given up on this time?</p>
    </div>
  </section>
</body>
</html>
)html";

std::string
slug_to_short(std::string_view slug)
{
    constexpr std::string_view delim {"-"};
    std::stringstream ss;
    std::vector<std::string_view> words;
    for (const auto &word : std::views::split(slug, delim) | std::views::drop(4)) {
        words.emplace_back(std::ranges::begin(word), std::ranges::end(word));
    }

    for (const auto &word : words | std::views::reverse |
             std::views::drop(2) | std::views::reverse | std::views::take(4))
    {
        ss << static_cast<char>(std::toupper(static_cast<unsigned char>(*word.begin())));
    }

    auto res = ss.str();
    if (res.empty()) { return "P1V1"; }
    if (res == "V1") return "P1V1";
    if (res == "V2") return "P1V2";
    return res;
}

std::string
slug_to_series_part(std::string_view slug)
{
    constexpr std::string_view delim {"-"};
    std::stringstream ss;
    std::vector<std::string_view> words;
    for (const auto &word : std::views::split(slug, delim) | std::views::drop(4)) {
        words.emplace_back(std::ranges::begin(word), std::ranges::end(word));
    }

    bool first = true;
    for (const auto &word : words | std::views::reverse |
             std::views::drop(2) | std::views::reverse | std::views::take(2))
    {
        ss << static_cast<char>(std::toupper(static_cast<unsigned char>(*word.begin())));
        ss << std::views::drop(word, 1);
        if (first) {
            ss << ' ';
            first = false;
        }
    }

    auto res = ss.str();
    if (res.empty()) { return "Part 1"; }
    if (res == "Volume 1") return "Part 1";
    if (res == "Volume 2") return "Part 1";
    return res;
}

std::string
slug_to_volume_part(std::string_view slug)
{
    constexpr std::string_view delim {"-"};
    std::stringstream ss;
    std::vector<std::string_view> words;
    for (const auto &word : std::views::split(slug, delim) | std::views::drop(4)) {
        words.emplace_back(std::ranges::begin(word), std::ranges::end(word));
    }

    if (words.empty()) return "1";
    return std::string(*std::ranges::begin(std::views::take(std::views::reverse(words), 1)));
}

void
write_gnuplot_part(std::string_view vol, std::filesystem::path filename, std::string_view y, const wordstat_map_t &stats)
{
    std::fstream latest(filename, latest.out);
    latest << "Words\ty\t" << std::quoted(vol) << "\t\"Volume Part\"\n";
    for (const auto &stat : stats) {
        if (slug_to_short(stat.first.second) != vol) continue;
        latest << stat.second << '\t' << y << '\t' << std::quoted(vol) << '\t' << std::quoted(slug_to_volume_part(stat.first.second)) << '\n';
    }
    latest.close();
}

void
write_gnuplot(const wordstat_map_t &stats, const std::filesystem::path& dir)
{

    std::fstream hist(dir / "hist.dat", hist.out);

    std::set<std::string> parts;
    hist << "Words\tPart\n";
    for (const auto &stat : stats) {
        hist << stat.second << '\t' << std::quoted(slug_to_series_part(stat.first.second)) << '\n';
        parts.insert(slug_to_short(stat.first.second));
    }
    hist.close();

    auto it = parts.crbegin();
    write_gnuplot_part(*it, dir / "latest.dat", "2.5", stats);
    ++it;
    write_gnuplot_part(*it, dir / "latest-1.dat", "5", stats);
    ++it;
    write_gnuplot_part(*it, dir / "latest-2.dat", "7.5", stats);
    ++it;
    write_gnuplot_part(*it, dir / "latest-3.dat", "10", stats);

}

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_SSL);

    if (argc < 4) { std::cerr << "Must pass path to word_stats.csv\n"; return EXIT_FAILURE; }
    std::filesystem::path stats_path_in { argv[1] };
    std::filesystem::path stats_path_out { argv[2] };
    std::filesystem::path gnuplot_dir { argv[3] };
    if (!std::filesystem::exists(stats_path_in)) { std::cerr << "Invalid path to word_stats.csv (in)\n"; return EXIT_FAILURE; }
    if (!std::filesystem::exists(gnuplot_dir)) { std::cerr << "Invalid path to gnuplot directory\n"; return EXIT_FAILURE; }
    if (!std::filesystem::is_directory(gnuplot_dir)) { std::cerr << "gnuplot directory isn't a directory\n"; return EXIT_FAILURE; }

    try {
        curl c;
        auto stats = read_wordstat_file(stats_path_in);
        /* Fetch any new data & store to stats_path_out */

        auto cred = login(c);
        auto ids = fetch_partlist(c, cred.auth_header, "ascendance-of-a-bookworm");
        bool modified = false;
        for (const auto& id : ids) {
            if (stats.contains(id)) continue;
            std::cout << "need to get " << id.second << '\n';
            std::stringstream ss;
            fetch_part(c, cred.auth_header, id, ss);
            auto cnt = count_words(ss);
            std::cout << "Got " << id.second << " (" << slug_to_short(id.second) << ")" << " which has " << cnt << " words\n";
            stats.emplace(id, cnt);
            modified = true;
        }
        if (modified) write_wordstat_file(stats_path_out, stats);

        /* Write gnuplot files */
        write_gnuplot(stats, gnuplot_dir);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    curl_global_cleanup();
    return EXIT_SUCCESS;
}
