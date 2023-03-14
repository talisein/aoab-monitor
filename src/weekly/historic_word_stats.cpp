#include "historic_word_stats.h"
#include "utils.h"

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
