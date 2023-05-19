#include "historic_word_stats.h"

#include <algorithm>
#include <concepts>
#include <ranges>
#include <cmath>

#include "facts.h"
#include "utils.h"
#include "model.h"

namespace {
constexpr auto BUCKET_SIZE{500};

auto
to_bucket(std::integral auto count) -> decltype(count)
{
    // bucket 5500 has count 5250-5749
    auto x = count + (BUCKET_SIZE / 2);
    // bucket 5500 has x 5500-5999
    return x - (x % BUCKET_SIZE);
}

constexpr std::array TEN_PART_VOLUMES{std::to_array<std::string_view>(
    {"P2V2", "P2V3", "P3V2", "P3V3", "P3V4", "P5V3"})};

bool
is_ten_part(std::string_view vol)
{
    return TEN_PART_VOLUMES.end() != std::ranges::find(TEN_PART_VOLUMES, vol);
};
bool
is_ten_part_slug(std::string_view slug)
{
    return is_ten_part(slug_to_short(slug));
}
bool
is_eight_part(std::string_view vol)
{
    return TEN_PART_VOLUMES.end() == std::ranges::find(TEN_PART_VOLUMES, vol);
};
bool
is_eight_part_slug(std::string_view slug)
{
    return is_eight_part(slug_to_short(slug));
}

constexpr auto EIGHT_PART_FILTER = std::views::filter(
    [](const auto &stat) { return is_eight_part_slug(stat.first.slug); });

constexpr auto TEN_PART_FILTER = std::views::filter(
    [](const auto &stat) { return is_ten_part_slug(stat.first.slug); });

auto
get_part_filter(std::string_view vol)
{
    return std::views::filter(
        [vol](const auto &stat) { return stat.first.get_short() == vol; });
}

bool
in_bucket(int words, int bucket_start)
{
    return words >= (bucket_start - (BUCKET_SIZE / 2)) &&
           words < (bucket_start + (BUCKET_SIZE / 2));
}
}  // namespace

historic_word_stats::historic_word_stats(std::istream &is) : modified(false)
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
        } catch (...) {
            throw;
        }
    }

    read(wordstats_is);
}

void
historic_word_stats::read(std::istream &is)
{
    for (std::string line; std::getline(is, line);) {
        // 12483,6384ed590517e1e171e6c551,ascendance-of-a-bookworm-part-5-volume-2-part-4
        constexpr std::string_view delim{","};
        auto r = std::views::split(line, delim);
        auto it = r.begin();
        int words;
        auto fromchars_res = std::from_chars(
            std::to_address(std::ranges::begin(*it)),
            std::to_address(std::ranges::end(*it)), words);
        if (fromchars_res.ec != std::errc()) {
            throw std::system_error(std::make_error_code(fromchars_res.ec),
                                    "Couldn't parse words int from csv");
        }
        ++it;
        if (it == std::ranges::end(r))
            throw std::system_error(
                std::make_error_code(std::errc::invalid_seek),
                "csv has less than 2 elements");
        auto legacy_id = std::string_view(std::ranges::begin(*it),
                                          std::ranges::end(*it));
        ++it;
        if (it == std::ranges::end(r))
            throw std::system_error(
                std::make_error_code(std::errc::invalid_seek),
                "csv has less than 3 elements");
        auto slug = std::string_view(std::ranges::begin(*it),
                                     std::ranges::end(*it));

        volume vol;
        vol.legacy_id = legacy_id;
        vol.slug = slug;
        wordstats.try_emplace(vol, words);
        volumes.emplace(vol.get_short());
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
            throw std::system_error(e.code(),
                                    "Can't open wordstat file for writing");
        } catch (...) {
            throw;
        }
        return;
    }

    write(wordstats_os);
    std::cout << "Wrote out " << filename << '\n';
}

void
historic_word_stats::write(std::ostream &os)
{
    for (const auto &stat : wordstats) {
        os << stat.second << ',' << stat.first.legacy_id << ','
           << stat.first.slug << '\n';
    }
}

/*
 * Produces points to plot on top of the histogram. The points should stack up
 * within each bucket. Will be called multiple times, so the stacking-up has to
 * continue (facilitated by seen_buckets list).
 *
 * Bucket  point   VolumeShortName   VolumePart     Words, e.g.
 * 8125     0.5        "P1V1"             1          7990
 *
 * Points are offset by 0.5 so they float above the histogram box boundary.
 */
void
historic_word_stats::write_histogram_volume_points(std::string_view vol,
                                                   std::ostream &ofs)
{
    ofs << "Words\ty\t" << std::quoted(vol) << "\t\"Volume Part\"\tWords\n";
    for (const auto &stat : wordstats | get_part_filter(vol)) {
        auto bucket = to_bucket(stat.second);
        /* */
        auto &seen_buckets = is_ten_part(vol) ? seen_buckets_10
                                              : seen_buckets_8;
        auto prev_part_cnt = std::ranges::count_if(
            wordstats, [&stat, bucket, vol](const auto &st) {
                return bucket == to_bucket(st.second) &&
                       (((st.first.get_series_part() <
                          stat.first.get_series_part()) &&
                         (is_ten_part(vol) ||
                          (is_eight_part(vol) &&
                           (!is_ten_part_slug(st.first.slug))))) ||
                        (!is_ten_part_slug(st.first.slug) && is_ten_part(vol)));
            });
        int y = prev_part_cnt + std::ranges::count(seen_buckets, bucket);
        seen_buckets.push_back(bucket);
        ofs << bucket + BUCKET_SIZE / 4 << '\t' << y << ".5\t"
            << std::quoted(vol) << '\t' << stat.first.get_volume_part() << '\t'
            << stat.second << '\n';
    }
}

void
historic_word_stats::write_histogram_volume_points(
    std::string_view vol, std::filesystem::path filename)
{
    std::fstream fs{filename, fs.out};
    write_histogram_volume_points(vol, fs);
    fs.close();
}

historic_word_stats::word_count_t
historic_word_stats::get_volume_total_words(std::string_view vol)
{
    historic_word_stats::word_count_t sum = 0;
    for (const auto &stat : wordstats | get_part_filter(vol)) {
        sum += stat.second;
    }
    return sum;
}

int
historic_word_stats::get_volume_last_part(std::string_view vol)
{
    auto r = std::views::reverse(wordstats | get_part_filter(vol));
    return std::stoi(std::ranges::begin(r)->first.get_volume_part());
}

historic_word_stats::word_count_t
historic_word_stats::get_volume_last_part_words(std::string_view vol)
{
    auto r = std::views::reverse(wordstats | get_part_filter(vol));
    return std::ranges::begin(r)->second;
}

/*
 * Writes two points to make a horizontal line where a volume average is.
 * If there's only 1 part in the volume so far, do not write any points
 */
void
historic_word_stats::write_volume_word_average(std::string_view vol,
                                               std::ostream &ofs)
{
    auto max_parts = is_eight_part(vol) ? 8 : 10;
    ofs << "Part\tWords\t\"" << vol << " Average\"\n";
    int sum = 0;
    auto r = wordstats | get_part_filter(vol);
    const int parts = std::ranges::distance(r);
    if (1 == parts) {  // Don't plot average if there's only 1 part published
        return;
    }
    for (const auto &stat : wordstats | get_part_filter(vol)) {
        sum += stat.second;
    }
    ofs << 1 << '\t' << sum / parts << '\n';
    ofs << max_parts << '\t' << sum / parts << '\n';
}

void
historic_word_stats::write_volume_word_average(std::string_view vol,
                                               std::filesystem::path filename)
{
    std::fstream fs{filename, fs.out};
    write_volume_word_average(vol, fs);
    fs.close();
}

void
historic_word_stats::write_projection(std::string_view cur_volume,
                                      std::string_view prev_volume,
                                      std::ostream &ofs)
{
    ofs << "Part\tWords\t\"" << cur_volume << " Projection\"\n";

    const int current_total_parts = is_eight_part_slug(cur_volume) ? 8 : 10;

    word_count_t current_last_part_words = get_volume_last_part_words(
        cur_volume);
    const int current_last_part = get_volume_last_part(cur_volume);
    if (current_last_part == current_total_parts)
        return; // Nothing to predict
    word_count_t current_total_words = get_volume_total_words(cur_volume);
    const auto current_jp_pages = aoab_facts::jp_page_lengths.at(cur_volume);

    auto model = models[current_last_part-1];
    std::cout << "Selected model " << model << '\n';
    std::cout << "current_total_words=" << current_total_words << ", jp_pages=" << current_jp_pages << '\n';
    const auto predicted_total_words = model.predict(current_total_words, current_jp_pages);
    const auto word_deficit = predicted_total_words - current_total_words;
    const auto remaining_parts = current_total_parts - current_last_part;
    const auto predicted_words_per_part = word_deficit / remaining_parts;
    std::cout << "Predicting\ttotal_words=" << predicted_total_words << ",\tword_deficit="
              << word_deficit << ",\twords_per_part=" << predicted_words_per_part << '\n';
    ofs << current_last_part << '\t' << current_last_part_words << '\n';
    for (int i = current_last_part + 1; i <= current_total_parts; ++i) {
        ofs << i << '\t' << predicted_words_per_part << '\n';
    }


    /* Old model */
    const auto previous_jp_pages = aoab_facts::jp_page_lengths.at(prev_volume);
    const auto ratio = current_jp_pages / previous_jp_pages;
    word_count_t previous_total_words = get_volume_total_words(prev_volume);
    std::cout << "Previously we would have predicted\n\t\ttotal_words="
              << previous_total_words * ratio << ",\tword_deficit="
              << static_cast<int>(previous_total_words * ratio) - current_total_words
              << ",\twords_per_part="
              << (static_cast<int>(previous_total_words * ratio) - current_total_words) / remaining_parts
              << '\n';
}

void
historic_word_stats::write_projection(std::string_view cur_volume,
                                      std::string_view prev_volume,
                                      std::filesystem::path filename)
{
    std::fstream fs{filename, fs.out};
    write_projection(cur_volume, prev_volume, fs);
    fs.close();
}
void
historic_word_stats::write_histogram(std::ostream &ofs)
{
    auto limits = get_limits();

    std::set<std::string> parts;
    std::ranges::copy(wordstats | std::views::transform([](const auto &stat) {
                          return stat.first.get_series_part();
                      }),
                      std::inserter(parts, parts.end()));
    ofs << "Bucket\t";
    for (const auto &x : parts) {
        ofs << std::quoted(x) << '\t';
    }
    ofs << std::quoted("10-part Part 2");
    ofs << '\t' << std::quoted("10-part Part 3");
    ofs << '\t' << std::quoted("10-part Part 5");
    ofs << '\n';

    auto eight_part_stats = wordstats | EIGHT_PART_FILTER;
    for (int bucket = limits.first; bucket < limits.second;
         bucket += BUCKET_SIZE) {
        ofs << bucket;
        for (const auto &part : parts) {
            auto cnt = std::ranges::count_if(
                eight_part_stats, [&part, bucket](const auto &stat) {
                    auto thispart = stat.first.get_series_part();
                    return part == thispart && in_bucket(stat.second, bucket);
                });
            ofs << '\t' << cnt;
        }
        for (const auto &part : {"Part 2", "Part 3", "Part 5"}) {
            ofs << '\t'
                << std::ranges::count_if(
                       TEN_PART_FILTER(wordstats),
                       [&part, bucket](const auto &stat) {
                           auto thispart = stat.first.get_series_part();
                           return thispart == part &&
                                  in_bucket(stat.second, bucket);
                       });
        }
        ofs << '\n';
    }
}

void
historic_word_stats::write_histogram(std::filesystem::path filename)
{
    std::fstream fs{filename, fs.out};
    write_histogram(fs);
    fs.close();
}

std::pair<historic_word_stats::word_count_t, historic_word_stats::word_count_t>
historic_word_stats::get_limits()
{
    auto [min, max] = std::ranges::minmax(
        wordstats |
        std::views::transform(&decltype(wordstats)::value_type::second));

    auto minrem = min % BUCKET_SIZE;
    auto maxrem = max % BUCKET_SIZE;
    auto maxadd = maxrem == 0 ? 0 : BUCKET_SIZE;

    // Round min down, round max up
    // 7301, 19158 -> 7000, 19500
    return {min - minrem, max + maxadd - maxrem};
}
