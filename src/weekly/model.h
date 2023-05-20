#pragma once
#include <cmath>
#include <iostream>

struct model
{
    using arith_t = double;
    int available_parts;
    arith_t available_words_slope;
    arith_t jp_pages_slope;
    arith_t intercept;

    arith_t words_avg;
    arith_t words_stdev;

    auto predict(auto available_words, auto jp_pages) -> decltype(available_words) {
        constexpr arith_t page_avg {389.7894737};
        constexpr arith_t page_stdev {24.55510571};

        const arith_t normalized_pages = (page_avg - static_cast<arith_t>(jp_pages)) / page_stdev;
        const arith_t normalized_words = (words_avg - static_cast<arith_t>(available_words)) / words_stdev;
        std::cout << "normalized_words=" << normalized_words << ", normalized_pages=" << normalized_pages << '\n';

        auto res = normalized_words * available_words_slope +
            normalized_pages * jp_pages_slope +
            intercept;
        return std::lround(res);
    }
};

constexpr auto models = std::to_array<model>({
        {1, 1082.655035, 7066.789665, 98336.89474, 11777.26316, 1715.418506},
        {2, 1991.503033, 6177.234192, 98336.89474, 23693.42105, 2948.079155},
        {3, 1538.934884, 6569.669045, 98336.89474, 36045.36842, 3220.923301},
        {4, 1756.461543, 6197.883029, 98336.89474, 48183.31579, 4094.773078},
        {5, 1890.534914, 5896.735764, 98336.89474, 61261.15789, 5079.944272},
        {6, 2925.338454, 4808.437858, 98336.89474, 73457.0,     6053.697539},
        {7, 4135.783294, 3641.93821,  98336.89474, 86294.63158, 6640.38053 }
    });

std::ostream& operator<<(std::ostream& os, const model& m)
{
    os << m.available_parts << " = [ "
       << m.available_words_slope << ", "
       << m.jp_pages_slope << ", "
       << m.intercept << " ]";

    return os;
}
