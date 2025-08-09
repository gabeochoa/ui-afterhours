#pragma once

// Ensure Raylib-backed types/macros are defined before any afterhours includes
#include "rl.h"

#include "afterhours/src/plugins/ui/context.h"
#include "afterhours/src/system.h"
#include "ui_demo/input_mapping.h"

struct DemoState : public afterhours::BaseComponent {
  size_t current_page_index = 0;
};

struct ExampleState : public afterhours::BaseComponent {
  bool showing = false;
  size_t screen_index = 0;
};

struct DemoRouter : afterhours::System<afterhours::ui::UIContext<InputAction>> {
  using UIX = afterhours::ui::UIContext<InputAction>;
  virtual void for_each_with(afterhours::Entity &entity, UIX &context,
                             float) override;
};
