#include <iostream>
#include <charconv>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <list>
#include <ranges>
#include <string_view>
#include <system_error>

using legacy_id_t = std::string;
using slug_t = std::string;
using id_map_t = std::map<legacy_id_t, slug_t>;

struct historic_word_stats
{
    using word_count_t = int;
    using legacy_slug_t = id_map_t::value_type;

    std::map<legacy_slug_t, word_count_t> wordstats;
    bool modified;
    std::set<std::string> volumes; // P1V1, P1V2, etc
    std::list<word_count_t> seen_buckets_8;
    std::list<word_count_t> seen_buckets_10;

private:
    void read(std::istream &is);
    std::pair<word_count_t, word_count_t> get_limits();

public:
    historic_word_stats(std::istream &is);

    historic_word_stats(std::filesystem::path filename);

    template<typename T>
    bool contains(T &&id) { return wordstats.contains(std::forward<T>(id)); }

    template<typename... Types>
    void emplace(Types&&... Ts) { auto [it, inserted] = wordstats.emplace(std::forward<Types>(Ts)...); modified = modified || inserted; }

    void write(std::filesystem::path filename);
    void write(std::ostream &os);


    void write_histogram(std::ostream &ofs);
    void write_histogram(std::filesystem::path filename);

    void write_histogram_volume_points(std::string_view vol, std::ostream &ofs);
    void write_histogram_volume_points(std::string_view vol, std::filesystem::path filename);

    void write_volume_word_average(std::string_view vol, std::ostream &ofs);
    void write_volume_word_average(std::string_view vol, std::filesystem::path filename);

    void write_projection(std::string_view cur_volume, std::string_view prev_volume, std::ostream &ofs);
    void write_projection(std::string_view cur_volume, std::string_view prev_volume, std::filesystem::path filename);

    word_count_t get_volume_total_words(std::string_view vol);

    int get_volume_last_part(std::string_view vol);
    word_count_t get_volume_last_part_words(std::string_view vol);
};
