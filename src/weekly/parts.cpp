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
#include <list>
#include <system_error>
#include "date/date.h"
#include "aoab_curl.h"
#include "defs.pb.h"
#include "facts.h"
#include "utils.h"
#include "historic_word_stats.h"

using namespace google::protobuf;

id_map_t
fetch_partlist(curl& c, curlslistp& auth_header, std::string_view volume_slug)
{
    std::stringstream url_ss;
    url_ss << "https://labs.j-novel.club/app/v1/parts/" << volume_slug << "/toc";
    auto url = url_ss.str();

    std::stringstream ss;

    c.set_get_opts(ss, auth_header, url);

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
fetch_part(curl& c, curlslistp& auth_header, const id_map_t::value_type& id, std::ostream &ofs)
{
    std::stringstream url_ss;
    url_ss << "https://labs.j-novel.club/embed/" << id.first << "/data.xhtml";
    auto url = url_ss.str();

    c.set_get_opts(ofs, auth_header, url);

    auto code = c.perform();
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to fetch library_response");
    }
}

using wordstat_map_t = decltype(historic_word_stats::wordstats);

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
        const auto ratio = aoab_facts::jp_page_lengths.at(current_jp_pages) / aoab_facts::jp_page_lengths.at(previous_jp_pages);
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
        auto stats = historic_word_stats{stats_path_in};
        /* Fetch any new data & store to stats_path_out */

        auto cred = c.login();
        auto ids = fetch_partlist(c, cred.auth_header, "ascendance-of-a-bookworm");
        for (const auto& id : ids) {
            if (stats.contains(id)) continue;
            std::cout << "need to get " << id.second << '\n';
            std::stringstream ss;
            fetch_part(c, cred.auth_header, id, ss);
            auto cnt = count_words(ss);
            std::cout << "Got " << id.second << " (" << slug_to_short(id.second) << ")" << " which has " << cnt << " words\n";
            stats.emplace(id, cnt);
        }
        if (stats.modified) stats.write(stats_path_out);

        /* Write gnuplot files */
        write_gnuplot(stats.wordstats, gnuplot_dir);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    curl_global_cleanup();
    return EXIT_SUCCESS;
}
