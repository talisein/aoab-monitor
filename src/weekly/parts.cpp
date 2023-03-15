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

void
write_gnuplot(historic_word_stats &wordstats, const std::filesystem::path& dir)
{
    auto &stats = wordstats.wordstats;

    auto prev_volume = *std::next(wordstats.volumes.crbegin());
    auto cur_volume = *wordstats.volumes.crbegin();

    wordstats.write_histogram_volume_points(prev_volume, dir / "latest-1.dat");
    wordstats.write_volume_word_average(prev_volume, dir / "latest-1-avg-1.dat");

    wordstats.write_histogram_volume_points(cur_volume, dir / "latest.dat");
    wordstats.write_volume_word_average(cur_volume, dir / "latest-avg-1.dat");

    wordstats.write_projection(cur_volume, prev_volume, dir / "latest-proj.dat");

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
        write_gnuplot(stats, gnuplot_dir);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    curl_global_cleanup();
    return EXIT_SUCCESS;
}
