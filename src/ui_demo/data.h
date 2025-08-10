#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ui_demo {
namespace data {

// Core page names for the demo router
extern const std::array<std::string_view, 3> kPageNames;

// Basic color options used in simple dropdowns or examples
extern const std::array<std::string_view, 3> kBasicColorOptions;

// Helpers to access std::vector<std::string> views of the static arrays
const std::vector<std::string> &page_names_vec();
const std::vector<std::string> &basic_color_options_vec();

} // namespace data
} // namespace ui_demo
