#include <charconv>
#include <sstream>
#include <string_view>
#include <vector>
#include <ranges>
#include "volume.h"
#include "utils.h"

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
    return slug_to_short(slug);
}

std::string
volume::get_series_part() const
{
    return slug_to_series_part(slug);
}

std::string
volume::get_volume_part() const
{
    return slug_to_volume_part(slug);
}

int
volume::get_volume_number() const
{
    return slug_to_volume_number(slug);
}
