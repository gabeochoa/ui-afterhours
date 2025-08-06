#include "backward/backward.hpp"

namespace backward {
backward::SignalHandling sh;
} // namespace backward

#include "rl.h"

#include "std_include.h"
#include <cassert>
//
// Use the wrapper log.h file that properly sets up the logging system
#include "log.h"

// Workaround for missing log_once_per function - must be defined before
// afterhours includes
#ifndef AFTER_HOURS_REPLACE_LOGGING
inline void log_once_per(...) {
  // Empty implementation to satisfy the linker
}
#endif

//
#define AFTER_HOURS_DEBUG
#define AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
#define AFTER_HOURS_REPLACE_LOGGING
#include <afterhours/ah.h>
#define AFTER_HOURS_USE_RAYLIB
#include "afterhours/src/developer.h"
#include "afterhours/src/font_helper.h"
#include "afterhours/src/plugins/autolayout.h"
#include "afterhours/src/plugins/color.h"
#include "afterhours/src/plugins/input_system.h"
#include "afterhours/src/plugins/texture_manager.h"
#include "afterhours/src/plugins/ui.h"
#include "afterhours/src/plugins/ui/immediate.h"
#include "afterhours/src/plugins/window_manager.h"

//
using namespace afterhours;

typedef raylib::Vector2 vec2;
typedef raylib::Vector3 vec3;
typedef raylib::Vector4 vec4;

constexpr float distance_sq(const vec2 a, const vec2 b) {
  return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

#define RectangleType raylib::Rectangle
// Remove the Vector2Type redefinition since it's already defined in rl.h

namespace myutil {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace myutil

struct EQ : public afterhours::EntityQuery<EQ> {};

struct RenderFPS : System<window_manager::ProvidesCurrentResolution> {
  virtual ~RenderFPS() {}
  virtual void for_each_with(
      const Entity &,
      const window_manager::ProvidesCurrentResolution &pCurrentResolution,
      float) const override {
    const window_manager::Resolution rez =
        pCurrentResolution.current_resolution;

    raylib::DrawFPS((int)(rez.width - 80), 0);
    raylib::DrawText(fmt::format("{}x{}", rez.width, rez.height).c_str(),
                     (int)(rez.width - 100), (int)80, (int)20,
                     raylib::RAYWHITE);
  }
};

enum class InputAction {
  None,
  WidgetNext,
  WidgetBack, // Add the missing WidgetBack enum value
  WidgetMod,
  WidgetPress,
  ValueDown,
  ValueUp,
};

using afterhours::input;

auto get_mapping() {
  std::map<InputAction, input::ValidInputs> mapping;
  mapping[InputAction::WidgetNext] = {
      raylib::KEY_TAB,
  };

  mapping[InputAction::WidgetBack] = {
      raylib::KEY_TAB, // Use TAB for both next and back with shift modifier
  };

  mapping[InputAction::WidgetPress] = {
      raylib::KEY_ENTER,
  };

  mapping[InputAction::ValueUp] = {
      raylib::KEY_UP,
  };
  mapping[InputAction::ValueDown] = {
      raylib::KEY_DOWN,
  };
  mapping[InputAction::WidgetMod] = {
      raylib::KEY_LEFT_SHIFT,
  };
  return mapping;
}

struct SetupUIStylingDefaults : System<afterhours::ui::UIContext<InputAction>> {
  bool setup_done = false;

  virtual void for_each_with(Entity &entity,
                             afterhours::ui::UIContext<InputAction> &context,
                             float) override {
    (void)entity;  // Mark as unused
    (void)context; // Mark as unused
    if (!setup_done) {
      using namespace afterhours::ui;
      using namespace afterhours::ui::imm;

      // Set up basic UI styling defaults
      auto &styling_defaults = UIStylingDefaults::get();
      (void)styling_defaults; // Mark as unused
      auto default_size = ComponentSize{pixels(200.f), pixels(50.f)};

      fmt::print("SetupUIStylingDefaults: Setting up UI styling defaults\n");
      setup_done = true;
    }
  }
};

struct SimpleUISystem : System<afterhours::ui::UIContext<InputAction>> {
  bool ui_created = false;

  virtual void for_each_with(Entity &entity,
                             afterhours::ui::UIContext<InputAction> &context,
                             float) override {
    using namespace afterhours::ui;
    using namespace afterhours::ui::imm;

    div(context, mk(entity),
        ComponentConfig().with_label("Hello from Afterhours!"));
    fmt::print("SimpleUISystem: UI context accessible, entity ID: {}\n",
               entity.id);
    fmt::print("SimpleUISystem: Entity has AutoLayoutRoot: {}\n",
               entity.has<ui::AutoLayoutRoot>());
  }
};

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight,
                     "UI Afterhours - Component Showcase");
  raylib::SetTargetFPS(200);

  // Create main entity
  auto &Sophie = EntityHelper::createEntity();
  {
    input::add_singleton_components<InputAction>(Sophie, get_mapping());
    window_manager::add_singleton_components(Sophie, 200);
    ui::add_singleton_components<InputAction>(Sophie);

    // Add AutoLayoutRoot component - required for UI elements
    Sophie.addComponent<ui::AutoLayoutRoot>();
    // Root UIComponent so children have a valid parent
    Sophie.addComponent<ui::UIComponent>(Sophie.id);
    Sophie.addComponent<ui::UIComponentDebug>("root");
    // Ensure newly added components are available this frame
    EntityHelper::merge_entity_arrays();
  }

  SystemManager systems;

  // debug systems
  {
    window_manager::enforce_singletons(systems);
    input::enforce_singletons<InputAction>(systems);
  }

  // external plugins
  {
    input::register_update_systems<InputAction>(systems);
    window_manager::register_update_systems(systems);
  }

  // UI systems - add them back but with proper singleton handling
  {
    ui::register_before_ui_updates<InputAction>(systems);
    {
      // Register our UI system between before and after UI updates
      systems.register_update_system(
          std::make_unique<SetupUIStylingDefaults>());
      systems.register_update_system(std::make_unique<SimpleUISystem>());
    }
    ui::register_after_ui_updates<InputAction>(systems);
  }

  // renders
  {
    systems.register_render_system(
        [&](float) { raylib::ClearBackground(raylib::DARKGRAY); });
    ui::register_render_systems<InputAction>(systems);
    systems.register_render_system(std::make_unique<RenderFPS>());
  }

  while (!raylib::WindowShouldClose()) {
    raylib::BeginDrawing();
    { systems.run(raylib::GetFrameTime()); }
    raylib::EndDrawing();
  }

  raylib::CloseWindow();

  return 0;
}
