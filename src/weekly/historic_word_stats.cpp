#include <ranges>
#include <algorithm>
#include <concepts>
#include "historic_word_stats.h"
#include "utils.h"

namespace {
    constexpr auto BUCKET_SIZE { 500 };

    auto to_bucket(std::integral auto count) -> decltype(count)
    {
        // bucket 5500 has count 5250-5749
        auto x = count + (BUCKET_SIZE/2);
        // bucket 5500 has x 5500-5999
        return x - (x % BUCKET_SIZE);
    }

    constexpr std::array TEN_PART_VOLUMES { std::to_array<std::string_view>({"P2V2", "P2V3", "P3V2", "P3V3", "P3V4", "P5V3"}) };

    bool is_ten_part(std::string_view vol) {
        return TEN_PART_VOLUMES.end() != std::ranges::find(TEN_PART_VOLUMES, vol);
    };
    bool is_ten_part_slug(std::string_view slug) {
        return is_ten_part(slug_to_short(slug));
    }
    bool is_eight_part(std::string_view vol) {
        return TEN_PART_VOLUMES.end() == std::ranges::find(TEN_PART_VOLUMES, vol);
    };
    bool is_eight_part_slug(std::string_view slug) {
        return is_eight_part(slug_to_short(slug));
    }

    constexpr auto EIGHT_PART_FILTER = std::views::filter(is_eight_part_slug);

    constexpr auto TEN_PART_FILTER = std::views::filter(is_ten_part_slug);

}

historic_word_stats::historic_word_stats(std::istream &is)
    : modified(false)
{
    read(is);
}

historic_word_stats::historic_word_stats(std::filesystem::path filename)
    : modified(false)
{
    std::fstream wordstats_is(filename, wordstats_is.in);
    if (!wordstats_is.is_open()) {
        try {
            wordstats_is.exceptions(wordstats_is.failbit | wordstats_is.badbit);
        } catch (std::ios_base::failure &e) {
            throw std::system_error(e.code(), "Can't open wordstat file");
        } catch (...) { throw; }
    }

    read(wordstats_is);
}

void
historic_word_stats::read(std::istream &is)
{
        for (std::string line; std::getline(is, line); ) {
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

            wordstats.try_emplace(id_map_t::value_type(legacy_id, slug), words);
            volumes.emplace(slug_to_short(slug));
        }
}

void
historic_word_stats::write(std::filesystem::path filename)
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

        write(wordstats_os);
        std::cout << "Wrote out " << filename << '\n';
}

void
historic_word_stats::write(std::ostream &os)
{
    for (const auto &stat : wordstats) {
        os << stat.second << ',' << stat.first.first << ',' << stat.first.second << '\n';
    }
}


/*
 * Produces points to plot on top of the histogram. The points should stack up
 * within each bucket. Will be called multiple times, so the stacking-up has to
 * continue (facilitated by seen_buckets list).
 *
 * Bucket  point   VolumeShortName   VolumePart, e.g.
 * 8125     0.5        "P1V1"             1
 *
 * Points are offset by 0.5 so they float above the histogram box boundary.
 *
 * Returns the wordcount of the last part in the volume.
 */
historic_word_stats::word_count_t
historic_word_stats::write_volume_points(std::string_view vol, std::ostream &ofs)
{
    word_count_t res = 0;
    ofs << "Words\ty\t" << std::quoted(vol) << "\t\"Volume Part\"\n";
    for (const auto &stat : wordstats) {
        if (slug_to_short(stat.first.second) != vol) continue;
        auto bucket = to_bucket(stat.second);
        /* */
        auto &seen_buckets = is_ten_part(vol) ? seen_buckets_10 : seen_buckets_8;
        auto prev_part_cnt = std::ranges::count_if(wordstats, [&stat, bucket, vol](const auto &st) {
            return bucket == to_bucket(st.second) &&
                (
                    (slug_to_series_part(st.first.second) < slug_to_series_part(stat.first.second)) ||
                    (!is_ten_part_slug(st.first.second) && is_ten_part(vol))
                );
        });
        int y = prev_part_cnt + std::ranges::count(seen_buckets, bucket);
        seen_buckets.push_back(bucket);
        ofs << bucket + BUCKET_SIZE/4 << '\t'
            << y << ".5\t"
            << std::quoted(vol) << '\t'
            << slug_to_volume_part(stat.first.second) << '\n';
        res = stat.second;
    }
    return res;
}

historic_word_stats::word_count_t
historic_word_stats::write_volume_points(std::string_view vol, std::filesystem::path filename)
{
    std::fstream fs {filename, fs.out};
    auto res = write_volume_points(vol, fs);
    fs.close();
    return res;
}
