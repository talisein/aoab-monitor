#include <boost/ut.hpp>
#include "utils.h"

int main() {
    using namespace boost::ut;
    using namespace std::literals;

    "slug to volume"_test = [&] {
        expect(eq(5, slug_to_volume_number("ascendence-part-4-volume-5-part-3"sv)));
        expect(eq(-1, slug_to_volume_number("ascendence-part-4-part-3"sv)));
        expect(eq(-1, slug_to_volume_number("ascendence-part-4-volume"sv)));
        expect(eq(-2, slug_to_volume_number("ascendence-part-4-volume-"sv)));
        expect(eq(-2, slug_to_volume_number("ascendence-part-4-volume-a"sv)));
        expect(eq(6, slug_to_volume_number("ascendence-part-4-volume-6"sv)));
        expect(eq(12, slug_to_volume_number("ascendence-part-4-volume-12-part-1"sv)));
    };

    "slug to short"_test = [&] {
        expect(eq("P5V1"sv, slug_to_short("ascendence-of-a-bookworm-part-5-volume-1-part-10"sv)));
        expect(eq("P5V10"sv, slug_to_short("ascendence-of-a-bookworm-part-5-volume-10-part-10"sv)));
    };

    "slug to series part"_test = [&] {
        expect(eq("Part 5"sv, slug_to_series_part("ascendence-of-a-bookworm-part-5-volume-1-part-10"sv)));
        expect(eq("Part 5"sv, slug_to_series_part("ascendence-of-a-bookworm-part-5-volume-10-part-10"sv)));
    };

    "slug to volume part"_test = [&] {
        expect(eq("10"sv, slug_to_volume_part("ascendence-of-a-bookworm-part-5-volume-1-part-10"sv)));
        expect(eq("10"sv, slug_to_volume_part("ascendence-of-a-bookworm-part-5-volume-10-part-10"sv)));
    };

    "slug to volume number"_test = [&] {
        expect(eq(1_i, slug_to_volume_number("ascendence-of-a-bookworm-part-5-volume-1-part-10"sv)));
        expect(eq(10_i, slug_to_volume_number("ascendence-of-a-bookworm-part-5-volume-10-part-10"sv)));
    };
}
