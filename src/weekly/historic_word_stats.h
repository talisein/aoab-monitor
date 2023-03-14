#include <iostream>
#include <charconv>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
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

private:
    void read(std::istream &is);

public:
    historic_word_stats(std::istream &is);

    historic_word_stats(std::filesystem::path filename);

    template<typename T>
    bool contains(T &&id) { return wordstats.contains(std::forward<T>(id)); }

    template<typename... Types>
    void emplace(Types&&... Ts) { auto [it, inserted] = wordstats.emplace(std::forward<Types>(Ts)...); modified = modified || inserted; }

    void write(std::filesystem::path filename);
    void write(std::ostream &os);

};
