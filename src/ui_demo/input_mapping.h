#pragma once

#include <map>
#include <vector>

#include "afterhours/src/plugins/input_system.h"
#include "rl.h"

enum class InputAction {
  None,
  WidgetNext,
  WidgetBack,
  WidgetMod,
  WidgetPress,
  ValueDown,
  ValueUp,
};

inline auto get_mapping() {
  using afterhours::input;
  std::map<InputAction, input::ValidInputs> mapping;
  mapping[InputAction::WidgetNext] = input::ValidInputs{raylib::KEY_TAB};
  mapping[InputAction::WidgetBack] = input::ValidInputs{raylib::KEY_TAB};
  mapping[InputAction::WidgetPress] = input::ValidInputs{raylib::KEY_ENTER};
  mapping[InputAction::ValueUp] = input::ValidInputs{raylib::KEY_UP};
  mapping[InputAction::ValueDown] = input::ValidInputs{raylib::KEY_DOWN};
  mapping[InputAction::WidgetMod] = input::ValidInputs{raylib::KEY_LEFT_SHIFT};
  return mapping;
}
