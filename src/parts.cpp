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

    using page_length_t = double;
    const std::map<std::string_view, page_length_t> jp_page_lengths {
        {"P5V1", 446},
        {"P5V2", 427},
        {"P5V3", 420},
        {"P5V4", 431},
        {"P5V5", 431},
        {"P5V6", 443},
        {"P5V7", 412},
        {"P5V8", 432},
        {"P5V9", 440},
        {"P5V10", 421},
    };
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
    std::cout << "Wrote out " << filename << '\n';
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

constexpr int BUCKET_SIZE = 500;
std::pair<int, int>
get_limits(const wordstat_map_t &stats)
{
    auto [min, max] = std::ranges::minmax(stats | std::views::transform(&wordstat_map_t::value_type::second));

    auto minrem = min % BUCKET_SIZE;
    auto maxrem = max % BUCKET_SIZE;
    auto maxadd = maxrem == 0 ? 0 : BUCKET_SIZE;

    // Round min down, round max up
    // 7301, 19158 -> 7000, 19500
    return {min - minrem, max + maxadd - maxrem};
}

bool
in_bucket(int words, int bucket_start)
{
    return words >= (bucket_start - (BUCKET_SIZE/2)) && words < (bucket_start + (BUCKET_SIZE/2));
}

constexpr std::array TEN_PART_VOLUMES { std::to_array<std::string_view>({"P2V2", "P2V3", "P3V2", "P3V3", "P3V4", "P5V3"}) };

constexpr auto EIGHT_PART_FILTER = std::views::filter([](const auto &stat) {
        auto vol = slug_to_short(stat.first.second);
        auto it = std::ranges::find(TEN_PART_VOLUMES, vol);
        return it == TEN_PART_VOLUMES.end();
});

constexpr auto TEN_PART_FILTER = std::views::filter([](const auto &stat) {
        auto vol = slug_to_short(stat.first.second);
        auto it = std::ranges::find(TEN_PART_VOLUMES, vol);
        return it != TEN_PART_VOLUMES.end();
});

void
make_histomap(const wordstat_map_t &stats, const std::filesystem::path& filename)
{
    std::fstream histo(filename, histo.out);
    auto limits = get_limits(stats);

    std::set<std::string> parts;
    std::ranges::copy(stats | std::views::transform([](const auto &stat) { return slug_to_series_part(stat.first.second); }), std::inserter(parts, parts.end()));
    histo << "Bucket\t";
    for (const auto &x : parts) {
        histo << std::quoted(x) << '\t';
    }
    histo << std::quoted("10-part Part 2");
    histo << '\t' << std::quoted("10-part Part 3");
    histo << '\t' << std::quoted("10-part Part 5");
    histo << '\n';

    auto eight_part_stats = stats | EIGHT_PART_FILTER;
    for (int bucket = limits.first; bucket < limits.second; bucket += BUCKET_SIZE)
    {
        histo << bucket << '\t';
        for (const auto &part : parts) {
            auto cnt = std::ranges::count_if(eight_part_stats, [&part, bucket](const auto &stat) { auto thispart = slug_to_series_part(stat.first.second); return part == thispart && in_bucket(stat.second, bucket); });
            histo << cnt << '\t';
        }
        for (const auto &part : {"Part 2", "Part 3", "Part 5"}) {
            histo <<  std::ranges::count_if(stats | TEN_PART_FILTER, [&part, bucket](const auto &stat) {
                auto thispart = slug_to_series_part(stat.first.second);
                return thispart == part && in_bucket(stat.second, bucket); }) << '\t';
        }
        histo << '\n';
    }
}

int to_bucket(int count)
{
    // bucket 5500 has count 5250-5749
    auto x = count + (BUCKET_SIZE/2);
    // bucket 5500 has x 5500-5999
    return x - (x % BUCKET_SIZE);
}

template<typename R>
int
write_gnuplot_part2(std::string_view vol, std::filesystem::path filename, R &stats, std::list<int> &seen_buckets)
{
    int res = 0;
    std::fstream latest(filename, latest.out);
    latest << "Words\ty\t" << std::quoted(vol) << "\t\"Volume Part\"\n";
    for (const auto &stat : stats) {
        if (slug_to_short(stat.first.second) != vol) continue;
        auto bucket = to_bucket(stat.second);
        auto prev_part_cnt = std::ranges::count_if(stats, [&stat, bucket](const auto &st){ return bucket == to_bucket(st.second) && slug_to_series_part(st.first.second) != slug_to_series_part(stat.first.second);});
        int y = prev_part_cnt + std::ranges::count(seen_buckets, bucket);
        seen_buckets.push_back(bucket);
        latest << bucket + BUCKET_SIZE/4 << '\t'
               << y << ".5\t"
               << std::quoted(vol) << '\t'
               << slug_to_volume_part(stat.first.second) << '\n';
        res = stat.second;
    }
    latest.close();
    return res;
}

auto get_part_filter(std::string_view vol)
{
    return std::views::filter([vol](const auto& stat) { return slug_to_short(stat.first.second) == vol; });
}

template<typename R>
int
write_gnuplot_accum(std::string_view vol, std::filesystem::path filename, R &stats)
{
    std::fstream fs(filename, fs.out);

    fs << "Part\tWords\t" << std::quoted(vol) << "\n0\t0\n";
    int sum = 0;
    for (const auto &stat : stats | get_part_filter(vol)) {
        sum += stat.second;
        fs << slug_to_volume_part(stat.first.second) << '\t';
        fs << sum << '\n';
    }
    fs.close();
    return sum;
}

template<typename R>
int
write_gnuplot_accum_summary(std::string_view vol, std::filesystem::path filename, R &stats)
{
    std::fstream fs(filename, fs.out);

    fs << "Part\tWords\t" << std::quoted(vol) << "\n0\t0\n";
    int sum = 0;
    std::string last_part;
    for (const auto &stat : stats | get_part_filter(vol)) {
        sum += stat.second;
        last_part = std::string(slug_to_volume_part(stat.first.second));
    }
    fs << last_part << '\t' << sum << '\n';
    fs.close();

    return sum;
}

template<typename R>
int
write_gnuplot_avg_summary(std::string_view vol, std::filesystem::path filename, R &stats, int max_parts)
{
    std::fstream fs(filename, fs.out);

    fs << "Part\tWords\t\"" << vol << " Average\"\n";
    int sum = 0;
    std::string last_part;
    auto r = stats | get_part_filter(vol);
    const int parts = std::ranges::distance(r);
    if (1 == parts) { // Don't plot average if there's only 1 part published
        return std::stoi(std::string(slug_to_volume_part(std::ranges::begin(r)->first.second)));
    }
    for (const auto &stat : stats | get_part_filter(vol)) {
        sum += stat.second;
        last_part = std::string(slug_to_volume_part(stat.first.second));
    }
    fs << 1 << '\t' << sum / parts << '\n';
    fs << max_parts << '\t' << sum / parts << '\n';
    fs.close();
    return std::stoi(last_part);
}

void
write_gnuplot(const wordstat_map_t &stats, const std::filesystem::path& dir)
{

    std::fstream hist(dir / "hist.dat", hist.out);

    std::set<std::string> volumes;
    hist << "Words\tPart\n";
    for (const auto &stat : stats) {
        hist << stat.second << '\t' << std::quoted(slug_to_series_part(stat.first.second)) << '\n';
        volumes.insert(slug_to_short(stat.first.second));
    }
    hist.close();

    auto r = stats | EIGHT_PART_FILTER;
    wordstat_map_t eight_part_stats {std::ranges::begin(r), std::ranges::end(r)};
    auto prev_volume = *std::next(volumes.crbegin());
    auto cur_volume = *volumes.crbegin();
    auto prev_stats = std::ranges::find(TEN_PART_VOLUMES, prev_volume) != std::ranges::end(TEN_PART_VOLUMES) ? stats : eight_part_stats;
    auto cur_stats = std::ranges::find(TEN_PART_VOLUMES, cur_volume) != std::ranges::end(TEN_PART_VOLUMES) ? stats : eight_part_stats;
//meow
    const int current_total_parts = std::ranges::find(TEN_PART_VOLUMES, cur_volume) != std::ranges::end(TEN_PART_VOLUMES) ? 10 : 8;
    const int prev_total_parts = std::ranges::find(TEN_PART_VOLUMES, prev_volume) != std::ranges::end(TEN_PART_VOLUMES) ? 10 : 8;
    const int max_total_parts = std::max(current_total_parts, prev_total_parts);

    std::list<int> seen_buckets;
    write_gnuplot_part2(prev_volume, dir / "latest-1.dat", prev_stats, seen_buckets);
    write_gnuplot_accum(prev_volume, dir / "latest-1-accum-1.dat", prev_stats);
    int previous_words = write_gnuplot_accum_summary(prev_volume, dir / "latest-1-accum-2.dat", prev_stats);
    write_gnuplot_avg_summary(prev_volume, dir / "latest-1-avg-1.dat", prev_stats, max_total_parts);

    write_gnuplot_accum(cur_volume, dir / "latest-accum-1.dat", cur_stats);
    int current_words = write_gnuplot_accum(cur_volume, dir / "latest-accum-2.dat", cur_stats);
    int last_point = write_gnuplot_part2(cur_volume, dir / "latest.dat", cur_stats, seen_buckets);
    int current_last_part = write_gnuplot_avg_summary(cur_volume, dir / "latest-avg-1.dat", cur_stats, max_total_parts);

    std::fstream fs(dir / "latest-proj.dat", fs.out);
    fs << "Part\tWords\t\"" << cur_volume << " Projection\"\n";
    if (current_last_part < current_total_parts && current_words < previous_words) {
        const auto current_jp_pages = *volumes.crbegin();
        const auto previous_jp_pages = *std::next(volumes.crbegin());
        const auto ratio = jp_page_lengths.at(current_jp_pages) / jp_page_lengths.at(previous_jp_pages);
        fs << current_last_part << '\t' << last_point << '\n';
        const int word_deficit = static_cast<int>(previous_words*ratio) - current_words;
        for (int i = current_last_part + 1; i <= current_total_parts; ++i) {
            fs << i << '\t' << word_deficit / (current_total_parts - current_last_part) << '\n';
        }
        fs.close();
    }

    /*
    ++it;
    write_gnuplot_part(*it, dir / "latest-2.dat", "7.5", stats);
    ++it;
    write_gnuplot_part(*it, dir / "latest-3.dat", "10", stats);
    */
    make_histomap(stats, dir / "histo.dat");
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
