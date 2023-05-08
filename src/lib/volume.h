#pragma once

#include <string>
#include "defs.pb.h"

struct volume
{
    volume() {};
    volume(std::string_view legacy_id, std::string_view slug) : legacy_id(legacy_id), slug(slug) {};
    volume(const jnovel::api::book&);
    volume(const jnovel::api::part&);

    std::string legacy_id;
    std::string slug;

    bool operator==(const volume& other) const {
        return legacy_id == other.legacy_id && slug == other.slug;
    }

    auto operator<=>(const volume& other) const {
        auto res = legacy_id <=> other.legacy_id;
        if (std::is_eq(res)) {
            return slug <=> other.slug;
        }
        return res;
    };

    std::string get_short() const; /* P1V1 */
    std::string get_series_part() const; /* Part 1 */
    std::string get_volume_part() const; /* */
    int get_volume_number() const; /* 1 */
};
