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
#include "volume.h"

using namespace google::protobuf;

std::vector<volume>
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

    jnovel::api::toc_response r;
    auto parsed = r.ParseFromIstream(&ss);
    if (!parsed) {
        throw std::runtime_error("failed to parse parts_response");
    }

    std::vector<volume> res;
//    res.reserve(r.parts().parts_size());
    for (auto &part : r.parts().parts()) {
        res.emplace_back(part);
    }

    return res;
}

void
fetch_part(curl& c, curlslistp& auth_header, const legacy_id_t& id, std::ostream &ofs)
{
    std::stringstream url_ss;
    url_ss << "https://labs.j-novel.club/embed/" << id << "/data.xhtml";
    auto url = url_ss.str();

    c.set_get_opts(ofs, auth_header, url);

    auto code = c.perform();
    if (CURLE_OK != code) {
        throw std::runtime_error("Failed to fetch library_response");
    }
}

void
write_gnuplot(historic_word_stats &wordstats, const std::filesystem::path& dir)
{
    auto prev_volume = *std::next(wordstats.volumes.crbegin());
    auto cur_volume = *wordstats.volumes.crbegin();

    std::cout << "Previous volume is " << prev_volume << '\n';
    std::cout << "Current  volume is " << cur_volume << '\n';

    wordstats.write_histogram_volume_points(prev_volume, dir / "latest-1.dat");
    wordstats.write_volume_word_average(prev_volume, dir / "latest-1-avg-1.dat");

    wordstats.write_histogram_volume_points(cur_volume, dir / "latest.dat");
    wordstats.write_volume_word_average(cur_volume, dir / "latest-avg-1.dat");

    wordstats.write_projection(cur_volume, prev_volume, dir / "latest-proj.dat");

    wordstats.write_histogram(dir / "histo.dat");
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
        auto stats = historic_word_stats {stats_path_in};

        /* Fetch any new data & store to stats_path_out */
        auto cred = c.login();
        auto volumes = fetch_partlist(c, cred.auth_header, "ascendance-of-a-bookworm");
        for (const auto& vol : volumes) {
            if (stats.contains(vol)) continue;
            std::cout << "need to get " << vol.slug << '\n';
            std::stringstream ss;
            fetch_part(c, cred.auth_header, vol.legacy_id, ss);
            auto cnt = count_words(ss);
            std::cout << "Got " << vol.slug << " (" << slug_to_short(vol.slug) << ")" << " which has " << cnt << " words\n";
            stats.emplace(vol, cnt);
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
