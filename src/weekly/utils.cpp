#include <ranges>
#include <sstream>
#include <libxml++/libxml++.h>
#include "utils.h"


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
