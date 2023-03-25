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

}
