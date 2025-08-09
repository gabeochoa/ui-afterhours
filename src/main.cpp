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

// Demo state stored on the root entity to hold current page selection, etc.
struct DemoState : public BaseComponent {
  size_t current_page_index = 0;
};

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

    // Root container
    auto root = div(context, mk(entity),
                    ComponentConfig().with_size(
                        ComponentSize{pixels(600.f), pixels(300.f)}));

    // Static state variables for interactive components
    static bool checkbox_value = false;
    static float slider_value = 0.5f;
    static size_t dropdown_index = 0;

    // Demonstrate each component type
    button(context, mk(root.ent(), 0), ComponentConfig().with_label("Press"));
    checkbox(context, mk(root.ent(), 1), checkbox_value,
             ComponentConfig().with_label("Check me"));
    slider(context, mk(root.ent(), 2), slider_value,
           ComponentConfig().with_label("Volume"));
    const std::vector<std::string> colors = {"Red", "Green", "Blue"};
    dropdown(context, mk(root.ent(), 3), colors, dropdown_index,
             ComponentConfig().with_label("Color"));
  }
};

// Router system: renders a navigation bar and routes to the active demo page
struct DemoRouter : System<afterhours::ui::UIContext<InputAction>> {
  using UIX = afterhours::ui::UIContext<InputAction>;

  virtual void for_each_with(Entity &entity, UIX &context, float) override {
    using namespace afterhours::ui;
    using namespace afterhours::ui::imm;

    // Ensure demo state exists
    DemoState &state = entity.addComponentIfMissing<DemoState>();

    // Page registry
    static const std::vector<std::string> page_names = {
        "Home",
        "Buttons",
        "Layout",
    };

    // Root container for all demo UI
    auto root = div(context, mk(entity, 1000),
                    ComponentConfig()
                        .with_size(ComponentSize{pixels(1100.f), pixels(650.f)})
                        .with_flex_direction(FlexDirection::Column)
                        .with_debug_name("demo_root"));

    // Top navigation bar
    size_t nav_index = state.current_page_index;
    navigation_bar(context, mk(root.ent(), 0), page_names, nav_index,
                   ComponentConfig()
                       .with_size(ComponentSize{percent(1.f), pixels(50.f)})
                       .with_flex_direction(FlexDirection::Row)
                       .with_debug_name("nav_bar"));
    state.current_page_index = nav_index;

    // Main content area
    auto content = div(context, mk(root.ent(), 1),
                       ComponentConfig()
                           .with_size(ComponentSize{percent(1.f), children()})
                           .with_flex_direction(FlexDirection::Column)
                           .with_debug_name("content"));

    // Render active page
    switch (state.current_page_index) {
    case 0: {
      // Home page: intro + compact gallery
      div(context, mk(content.ent(), 0),
          ComponentConfig()
              .with_label(
                  "Afterhours UI Demo: Use the nav bar to switch pages.")
              .with_size(ComponentSize{children(), pixels(50.f)})
              .with_skip_tabbing(true)
              .with_debug_name("home_intro"));

      auto gallery = div(context, mk(content.ent(), 1),
                         ComponentConfig()
                             .with_size(ComponentSize{percent(1.f), children()})
                             .with_flex_direction(FlexDirection::Row)
                             .with_debug_name("home_gallery"));

      // Mini-gallery widgets
      static bool home_checkbox = false;
      static float home_slider = 0.25f;
      static size_t home_dd = 0;
      const std::vector<std::string> dd_opts = {"Red", "Green", "Blue"};

      button(context, mk(gallery.ent(), 0),
             ComponentConfig().with_label("Button"));
      checkbox(context, mk(gallery.ent(), 1), home_checkbox,
               ComponentConfig().with_label("Checkbox"));
      slider(context, mk(gallery.ent(), 2), home_slider,
             ComponentConfig().with_label("Slider"));
      dropdown(context, mk(gallery.ent(), 3), dd_opts, home_dd,
               ComponentConfig().with_label("Dropdown"));
      break;
    }
    case 1: {
      // Buttons page: buttons and button groups
      div(context, mk(content.ent(), 0),
          ComponentConfig()
              .with_label("Buttons & Button Groups")
              .with_size(ComponentSize{children(), pixels(40.f)})
              .with_skip_tabbing(true)
              .with_debug_name("buttons_header"));

      auto row = div(context, mk(content.ent(), 1),
                     ComponentConfig()
                         .with_size(ComponentSize{percent(1.f), children()})
                         .with_flex_direction(FlexDirection::Row)
                         .with_debug_name("buttons_row"));

      button(context, mk(row.ent(), 0),
             ComponentConfig().with_label("Primary"));
      button(context, mk(row.ent(), 1),
             ComponentConfig().with_label("Disabled").with_disabled(true));

      const std::vector<std::string> labels = {"One", "Two", "Three"};
      button_group(context, mk(row.ent(), 2), labels,
                   ComponentConfig()
                       .with_size(ComponentSize{pixels(300.f), pixels(50.f)})
                       .with_flex_direction(FlexDirection::Row)
                       .with_debug_name("btn_group_row"));

      button_group(context, mk(content.ent(), 2), labels,
                   ComponentConfig()
                       .with_size(ComponentSize{pixels(200.f), children()})
                       .with_flex_direction(FlexDirection::Column)
                       .with_debug_name("btn_group_col"));
      break;
    }
    case 2: {
      // Layout page: rows, columns, sizing
      div(context, mk(content.ent(), 0),
          ComponentConfig()
              .with_label("Layout: Row/Column, Percent/Pixel sizes")
              .with_size(ComponentSize{children(), pixels(40.f)})
              .with_skip_tabbing(true)
              .with_debug_name("layout_header"));

      auto row = div(context, mk(content.ent(), 1),
                     ComponentConfig()
                         .with_size(ComponentSize{percent(1.f), pixels(80.f)})
                         .with_flex_direction(FlexDirection::Row)
                         .with_debug_name("layout_row"));
      div(context, mk(row.ent(), 0),
          ComponentConfig()
              .with_label("33%")
              .with_size(ComponentSize{percent(0.33f), children()})
              .with_debug_name("layout_row_a"));
      div(context, mk(row.ent(), 1),
          ComponentConfig()
              .with_label("34%")
              .with_size(ComponentSize{percent(0.34f), children()})
              .with_debug_name("layout_row_b"));
      div(context, mk(row.ent(), 2),
          ComponentConfig()
              .with_label("33%")
              .with_size(ComponentSize{percent(0.33f), children()})
              .with_debug_name("layout_row_c"));

      auto column = div(context, mk(content.ent(), 2),
                        ComponentConfig()
                            .with_size(ComponentSize{children(), children()})
                            .with_flex_direction(FlexDirection::Column)
                            .with_debug_name("layout_column"));
      div(context, mk(column.ent(), 0),
          ComponentConfig()
              .with_label("100px")
              .with_size(ComponentSize{children(), pixels(100.f)})
              .with_debug_name("layout_col_a"));
      div(context, mk(column.ent(), 1),
          ComponentConfig()
              .with_label("children() height")
              .with_size(ComponentSize{children(), children()})
              .with_debug_name("layout_col_b"));
      break;
    }
    default:
      break;
    }
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
    // Set desired starting resolution
    window_manager::Resolution startRez{1920, 1080};
    window_manager::add_singleton_components(Sophie, startRez, 200);
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
      systems.register_update_system(std::make_unique<DemoRouter>());
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
