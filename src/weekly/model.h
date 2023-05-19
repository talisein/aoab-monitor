#pragma once
#include <cmath>

struct model
{
    using arith_t = double;
    int available_parts;
    arith_t available_words_slope;
    arith_t jp_pages_slope;
    arith_t intercept;

    auto predict(auto available_words, auto jp_pages) -> decltype(available_words) {
        auto res = available_words * available_words_slope +
            jp_pages * jp_pages_slope +
            intercept;
        return std::lround(res);
    }
};

constexpr auto models = std::to_array<model>({
        {1, 0.2633081167, 244.5488742, 0},
        {2, 0.6251473025, 214.4604813, 0},
        {3, 0.3274302394, 222.2450806, 0},
        {4, 0.3394691099, 210.5438255, 0},
        {5, 0.3406427501, 198.9480125, 0},
        {6, 0.518256393, 154.7629059, 0},
        {7, 0.6612492242, 106.0296633, 0}
    });

std::ostream& operator<<(std::ostream& os, const model& m)
{
    os << m.available_parts << " = [ "
       << m.available_words_slope << ", "
       << m.jp_pages_slope << ", "
       << m.intercept << " ]";

    return os;
}
