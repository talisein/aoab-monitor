#include <ranges>
#include <sstream>
#include <libxml++/libxml++.h>
#include <charconv>
#include "utils.h"

#include <iostream>

class word_parser : public xmlpp::SaxParser
{
    public:
    word_parser(std::istream &ss) { parse_stream(ss); };

    virtual ~word_parser() override {};

    std::ranges::range_difference_t<std::ranges::istream_view<std::string>> count() {
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


std::istream::off_type
count_words(std::istream& ss)
{
    word_parser p { ss };
    return {p.count()};
}

/* slug to P1V1 */
std::string
slug_to_short(std::string_view slug)
{
    constexpr std::string_view delim {"-"};
    std::stringstream ss;
    std::vector<std::string_view> words;
    for (const auto &word : std::views::split(slug, delim) | std::views::drop(4)) {
        words.emplace_back(std::ranges::begin(word), std::ranges::end(word));
    }

    auto view = words | std::views::reverse | std::views::drop(2) | std::views::reverse;

    auto it = view.begin();
    if (it == view.end()) return "P1V1";
    ss << static_cast<char>(std::toupper(static_cast<unsigned char>(*it->begin())));
    it = std::next(it);
    ss << *it;
    it = std::next(it);
    if (it == view.end()) {
        auto res = ss.str();
        if (res == "V1") return "P1V1";
        if (res == "V2") return "P1V2";
        return "UNKNOWN-P?V?";
    }
    ss << static_cast<char>(std::toupper(static_cast<unsigned char>(*it->begin())));
    ss << *std::next(it);

    auto res = ss.str();
    return res;
}

/* slug to Part 1 */
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

int
slug_to_volume_number(std::string_view slug)
{
    constexpr std::string_view delim {"-"};
    auto view = std::views::split(slug, delim) | std::views::transform([](auto in) { return std::string_view(in.begin(), in.end()); });
    using namespace std::string_view_literals;
    auto it = std::ranges::find(view, "volume"sv);
    std::ranges::advance(it, 1, std::ranges::end(view));
    if (std::ranges::end(view) == it)
        return -1;

    int res;
    auto sv = *it;
    auto fc_res = std::from_chars(std::to_address(sv.begin()), std::to_address(sv.end()), res);
    if (fc_res.ec != std::errc())
        return -2;
    return res;
}
