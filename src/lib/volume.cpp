#include <charconv>
#include <sstream>
#include <string_view>
#include <vector>
#include <ranges>
#include "volume.h"

volume::volume(const jnovel::api::book& book) :
    legacy_id(book.volume().legacyid()),
    slug(book.volume().slug())
{
}

volume::volume(const jnovel::api::part& part) :
    legacy_id(part.legacyid()),
    slug(part.slug())
{
}

std::string
volume::get_short() const
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
volume::get_series_part() const
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
volume::get_volume_part() const
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
volume::get_volume_number() const
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
