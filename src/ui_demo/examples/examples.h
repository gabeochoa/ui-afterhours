#pragma once

// Ensure Raylib-backed types/macros are defined before any afterhours includes
#include "rl.h"
#include "afterhours/src/plugins/ui/context.h"
#include "afterhours/src/system.h"
#include "ui_demo/input_mapping.h"

namespace ui_demo {
namespace examples {

using UIX = afterhours::ui::UIContext<InputAction>;

// Declarations for example entrypoints
void render_single_button(UIX &context, afterhours::Entity &panel);

} // namespace examples
} // namespace ui_demo

