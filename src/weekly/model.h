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
        {1, 0.3659719584, 293.4247879, -20331.70354},
        {2, 0.5646460071, 259.1448056, -16039.77902},
        {3, 0.4403884838, 271.7502879, -23448.16944},
        {4, 0.3812260323, 258.8038596, -20897.23854},
        {5, 0.3307877444, 247.9782054, -18573.78817},
        {6, 0.4529330856, 203.1007251, -14090.04616},
        {7, 0.6091797018, 152.6156523, -13711.98558}
    });

std::ostream& operator<<(std::ostream& os, const model& m)
{
    os << m.available_parts << " = [ "
       << m.available_words_slope << ", "
       << m.jp_pages_slope << ", "
       << m.intercept << " ]";

    return os;
}
