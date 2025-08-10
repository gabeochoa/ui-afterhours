#include "data.h"

namespace ui_demo {
namespace data {

const std::array<std::string_view, 3> kPageNames = {
    std::string_view{"Home"}, std::string_view{"Buttons"},
    std::string_view{"Layout"}};

const std::array<std::string_view, 3> kBasicColorOptions = {
    std::string_view{"Red"}, std::string_view{"Green"},
    std::string_view{"Blue"}};

static std::vector<std::string> to_vec(const std::array<std::string_view, 3> &in) {
  std::vector<std::string> out;
  out.reserve(in.size());
  for (auto sv : in) out.emplace_back(sv);
  return out;
}

const std::vector<std::string> &page_names_vec() {
  static const std::vector<std::string> v = to_vec(kPageNames);
  return v;
}

const std::vector<std::string> &basic_color_options_vec() {
  static const std::vector<std::string> v = to_vec(kBasicColorOptions);
  return v;
}

} // namespace data
} // namespace ui_demo

