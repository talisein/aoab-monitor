#include <boost/ut.hpp>
#include <spanstream>
#include <algorithm>
#include "historic_word_stats.h"

int main() {
    using namespace boost::ut;

    constexpr std::string_view historic_test_data { R"(11918,5c8df94d4aa7a7a541cac4ce,ascendance-of-a-bookworm
7990,5c8dfa84a72a659641fb0110,ascendance-of-a-bookworm-volume-1-part-2
11460,5c990895483da0f204567935,ascendance-of-a-bookworm-volume-1-part-3
10297,5c99097200e9aa425866d59e,ascendance-of-a-bookworm-volume-1-part-4
9829,5ca332c2718aede3047a68c4,ascendance-of-a-bookworm-volume-1-part-5
8081,5cab6f878f6a254f48cbf6eb,ascendance-of-a-bookworm-volume-1-part-6
14204,5cb62af718bf8ced76bc544f,ascendance-of-a-bookworm-volume-1-part-7
9957,5cbe8990d668338d4f008bb0,ascendance-of-a-bookworm-volume-1-part-8
12249,5cd4c852aa10ee434f3fd7aa,ascendance-of-a-bookworm-volume-2-part-1
12410,5cdb8260b7ba145b7e35440b,ascendance-of-a-bookworm-part-1-volume-2-part-2
)"};

    "read"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};

        expect(stats.contains(historic_word_stats::legacy_slug_t{"5cdb8260b7ba145b7e35440b","ascendance-of-a-bookworm-part-1-volume-2-part-2"}));
        expect(!stats.contains(historic_word_stats::legacy_slug_t{"cdb8260b7ba145b7e35440b","ascendance-of-a-bookworm-part-1-volume-2-part-2"}));

        expect(std::ranges::equal(stats.volumes, std::initializer_list<std::string_view>{"P1V1", "P1V2"}));
    };

    "write"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::array<char, std::size(historic_test_data)> buf;
        std::ospanstream os {buf};
        stats.write(os);
        expect(std::ranges::equal(buf, historic_test_data));
        expect(!stats.modified);

        stats.emplace(historic_word_stats::legacy_slug_t{"1abc","slug"}, 2);
        std::ospanstream os2 {buf};
        stats.write(os2);
        expect(!std::ranges::equal(buf, historic_test_data));
        expect(stats.modified);
    };

}
