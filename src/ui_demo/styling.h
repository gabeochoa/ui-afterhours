#pragma once

#include "afterhours/src/plugins/ui/immediate.h"

struct SetupUIStylingDefaults
    : afterhours::System<afterhours::ui::UIContext<InputAction>> {
  bool setup_done = false;
  virtual void for_each_with(afterhours::Entity &,
                             afterhours::ui::UIContext<InputAction> &context,
                             float) override {
    (void)context;
    if (setup_done)
      return;
    using namespace afterhours::ui::imm;
    auto &styling_defaults = UIStylingDefaults::get();
    (void)styling_defaults;
    setup_done = true;
  }
};
