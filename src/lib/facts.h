#include <map>
#include <string_view>

namespace aoab_facts
{

    using page_length_t = double;
    const std::map<std::string_view, page_length_t> jp_page_lengths {
        {"P1V1", 450}, // for test
        {"P1V2", 400},
        {"P5V1", 446},
        {"P5V2", 427},
        {"P5V3", 420},
        {"P5V4", 431},
        {"P5V5", 431},
        {"P5V6", 443},
        {"P5V7", 412},
        {"P5V8", 432},
        {"P5V9", 440},
        {"P5V10", 421},
    };

}
