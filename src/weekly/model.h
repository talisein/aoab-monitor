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
        constexpr arith_t page_avg {389.8636364};
        constexpr arith_t page_stdev {23.11934967};

        const arith_t normalized_pages = (static_cast<arith_t>(jp_pages) - page_avg) / page_stdev;
        const arith_t normalized_words = (static_cast<arith_t>(available_words) - words_avg) / words_stdev;
        std::cout << "normalized_words=" << normalized_words << ", normalized_pages=" << normalized_pages << '\n';

        auto res = normalized_words * available_words_slope +
            normalized_pages * jp_pages_slope +
            intercept;
        return std::lround(res);
    }
};

constexpr auto models = std::to_array<model>({
        {1, 864.8917669, 6785.575785, 97907.36364, 11853.5, 1618.924297},
        {2, 1592.49897, 6059.165117, 97907.36364, 23868.04545, 2802.932418},
        {3, 1323.883936, 6312.936342, 97907.36364, 36200.63636, 3097.640093},
        {4, 1789.278928, 5814.22287, 97907.36364, 48135.95455, 3927.048746},
        {5, 2061.611666, 5458.541954, 97907.36364, 60939.95455, 5080.609548},
        {6, 2923.953144, 4513.612987, 97907.36364, 73180.77273, 5967.193557},
        {7, 4106.626635, 3382.70746, 97907.36364, 85985.95455, 6542.823648}
    });

std::ostream& operator<<(std::ostream& os, const model& m)
{
    os << m.available_parts << " = [ "
       << m.available_words_slope << ", "
       << m.jp_pages_slope << ", "
       << m.intercept << " ]";

    return os;
}
