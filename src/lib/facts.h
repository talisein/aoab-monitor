#include <map>
#include <string_view>

namespace aoab_facts
{

    using page_length_t = double;
    const std::map<std::string_view, page_length_t> jp_page_lengths {
        {"P1V1", 450}, // for test
        {"P1V2", 400},
        {"P2V2", 450}, // for test
        {"P5V1", 405},
        {"P5V2", 389},
        {"P5V3", 381},
        {"P5V4", 391},
        {"P5V5", 395},
        {"P5V6", 401},
        {"P5V7", 375},
        {"P5V8", 397},
        {"P5V9", 411},
        {"P5V10", 400}, // ??
        {"P5V11", 400}, // ??
        {"P5V12", 400}, // ??
    };

}
