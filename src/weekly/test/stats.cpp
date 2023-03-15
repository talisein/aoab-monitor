#include <boost/ut.hpp>
#include <spanstream>
#include <algorithm>
#include "historic_word_stats.h"

int main() {
    using namespace boost::ut;
    using namespace std::literals;

    constexpr std::string_view historic_test_data { R"(11918,5c8df94d4aa7a7a541cac4ce,ascendance-of-a-bookworm
7990,5c8dfa84a72a659641fb0110,ascendance-of-a-bookworm-volume-1-part-2
11460,5c990895483da0f204567935,ascendance-of-a-bookworm-volume-1-part-3
10297,5c99097200e9aa425866d59e,ascendance-of-a-bookworm-volume-1-part-4
9829,5ca332c2718aede3047a68c4,ascendance-of-a-bookworm-volume-1-part-5
8081,5cab6f878f6a254f48cbf6eb,ascendance-of-a-bookworm-volume-1-part-6
14204,5cb62af718bf8ced76bc544f,ascendance-of-a-bookworm-volume-1-part-7
9957,5cbe8990d668338d4f008bb0,ascendance-of-a-bookworm-volume-1-part-8
12249,5cd4c852aa10ee434f3fd7aa,ascendance-of-a-bookworm-volume-2-part-1
12410,6cdb8260b7ba145b7e35441b,ascendance-of-a-bookworm-part-1-volume-2-part-2
12249,7cdb8260b7ba145b7e35442b,ascendance-of-a-bookworm-part-2-volume-1-part-1
12249,8cdb8260b7ba145b7e35443b,ascendance-of-a-bookworm-part-2-volume-2-part-1
12249,9cdb8260b7ba145b7e35443b,ascendance-of-a-bookworm-part-2-volume-4-part-1
12249,9ddb8260b7ba145b7e35443b,ascendance-of-a-bookworm-part-3-volume-1-part-1
)"};

    "read"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};

        expect(stats.contains(historic_word_stats::legacy_slug_t{"6cdb8260b7ba145b7e35441b","ascendance-of-a-bookworm-part-1-volume-2-part-2"}));
        expect(!stats.contains(historic_word_stats::legacy_slug_t{"cdb8260b7ba145b7e35440b","ascendance-of-a-bookworm-part-1-volume-2-part-2"}));

        expect(std::ranges::equal(stats.volumes, std::initializer_list<std::string_view>{"P1V1", "P1V2", "P2V1", "P2V2", "P2V4", "P3V1"}));
    };

    "write"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream os;
        stats.write(os);
        expect(eq(os.str(), historic_test_data));
        expect(!stats.modified);

        os.str("");
        stats.emplace(historic_word_stats::legacy_slug_t{"1abc","slug"}, 2);
        stats.write(os);
        expect(neq(os.str(), historic_test_data));
        expect(stats.modified);
    };

    "write_histogram_volume_points"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream ofs;
        stats.write_histogram_volume_points("P1V1"sv, ofs);
        expect(eq(ofs.str(), R"(Words	y	"P1V1"	"Volume Part"	Words
12125	0.5	"P1V1"	1	11918
8125	0.5	"P1V1"	2	7990
11625	0.5	"P1V1"	3	11460
10625	0.5	"P1V1"	4	10297
10125	0.5	"P1V1"	5	9829
8125	1.5	"P1V1"	6	8081
14125	0.5	"P1V1"	7	14204
10125	1.5	"P1V1"	8	9957
)"sv));

        ofs.str("");
        stats.write_histogram_volume_points("P1V2"sv, ofs);
        expect(eq(ofs.str(), R"(Words	y	"P1V2"	"Volume Part"	Words
12125	1.5	"P1V2"	1	12249
12625	0.5	"P1V2"	2	12410
)"sv));
    };

    "write_histogram_volume_points_part2"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream ofs;
        stats.write_histogram_volume_points("P2V1"sv, ofs);
        /* There are 2 points in this bucket from Part 1,
         * so need to start from 2 */
        expect(eq(ofs.str(), R"(Words	y	"P2V1"	"Volume Part"	Words
12125	2.5	"P2V1"	1	12249
)"sv));

        /* P2V2 is a 10-part volume. 10-parts above all 8-parts. */
        ofs.str("");
        stats.write_histogram_volume_points("P2V2"sv, ofs);
        expect(eq(ofs.str(), R"(Words	y	"P2V2"	"Volume Part"	Words
12125	5.5	"P2V2"	1	12249
)"sv));
    };

    "write_histogram_volume_points_10part"_test = [&] {
        /* P2V2 is a 10-part volume. 10-part boxes are placed above all 8-part boxes. */
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream ofs;
        stats.write_histogram_volume_points("P2V2"sv, ofs);
        /* */
        expect(eq(ofs.str(), R"(Words	y	"P2V2"	"Volume Part"	Words
12125	5.5	"P2V2"	1	12249
)"sv));

        /* P2V4 is an 8-part volume. There are 2 points from Part 1, so we are
         * above that. But this function hasn't run on P2V1, so we start from the bottom of that box */
        ofs.str("");
        stats.write_histogram_volume_points("P2V4"sv, ofs);
        expect(eq(ofs.str(), R"(Words	y	"P2V4"	"Volume Part"	Words
12125	2.5	"P2V4"	1	12249
)"sv));
    };

    "write_histogram_volume_points_8part"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream ofs;
        stats.write_histogram_volume_points("P3V1"sv, ofs);
        /* Must not count 10-part P2V2 */
        expect(eq(ofs.str(), R"(Words	y	"P3V1"	"Volume Part"	Words
12125	4.5	"P3V1"	1	12249
)"sv));
    };

    "write_volume_word_average"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream ofs;
        stats.write_volume_word_average("P1V1"sv, ofs);

        expect(eq(ofs.str(), R"(Part	Words	"P1V1 Average"
1	10467
8	10467
)"sv));

        ofs.str("");
        stats.write_volume_word_average("P3V1"sv, ofs);
        expect(eq(ofs.str(), R"(Part	Words	"P3V1 Average"
)"sv));
    };

    "write_projection"_test = [&] {
        std::ispanstream is {historic_test_data};
        historic_word_stats stats {is};
        std::ostringstream ofs;
        stats.write_projection("P1V2"sv, "P1V1"sv, ofs);

        expect(eq(ofs.str(), R"(Part	Words	"P1V2 Projection"
2	12410
3	8295
4	8295
5	8295
6	8295
7	8295
8	8295
)"sv));
    };
}
