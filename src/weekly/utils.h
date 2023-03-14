#include <istream>
#include <string>
#include <string_view>

std::istream::off_type count_words(std::istream& ss);

std::string slug_to_short(std::string_view slug);
std::string slug_to_series_part(std::string_view slug);
std::string slug_to_volume_part(std::string_view slug);
