#include "backward/backward.hpp"

namespace backward {
backward::SignalHandling sh;
} // namespace backward

#include "rl.h"

#include "std_include.h"
#include <cassert>
//
// Use the wrapper log.h file that properly sets up the logging system
#include "argh.h"
#include "log.h"
#include "magic_enum/magic_enum.hpp"
#include "nlohmann/json.hpp"
#include "toml.hpp"

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
struct ExampleState : public BaseComponent {
  bool showing = false;
  size_t screen_index = 0;
};

// Minimal test action playback support driven by a simple TOML file
struct PlaybackStep {
  std::vector<InputAction> pressed;
  std::vector<InputAction> held;
};

struct PlaybackConfig {
  std::vector<PlaybackStep> steps;
  bool auto_quit = false;
  std::string dump_path = "ui_tree.json";
};

static std::optional<PlaybackConfig> g_playback_config;
static std::atomic<bool> g_should_quit{false};

static std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  size_t b = s.find_last_not_of(" \t\r\n");
  if (a == std::string::npos)
    return "";
  return s.substr(a, b - a + 1);
}

static std::vector<std::string> parse_string_array(const std::string &line) {
  std::vector<std::string> out;
  auto l = line.find('[');
  auto r = line.find(']');
  if (l == std::string::npos || r == std::string::npos || r <= l)
    return out;
  std::string inside = line.substr(l + 1, r - l - 1);
  std::stringstream ss(inside);
  std::string token;
  while (std::getline(ss, token, ',')) {
    token = trim(token);
    if (!token.empty() && token.front() == '"' && token.back() == '"') {
      token = token.substr(1, token.size() - 2);
    }
    if (!token.empty())
      out.push_back(token);
  }
  return out;
}

static std::optional<PlaybackConfig>
load_actions_toml(const std::string &path) {
  auto parse_action =
      [](const std::string &name) -> std::optional<InputAction> {
    if (auto a = magic_enum::enum_cast<InputAction>(name))
      return a;
    // Try with potential namespace prefix trimmed
    const std::string prefix = "InputAction::";
    if (name.rfind(prefix, 0) == 0) {
      std::string trimmed = name.substr(prefix.size());
      if (auto a = magic_enum::enum_cast<InputAction>(trimmed))
        return a;
    }
    // Case-insensitive fallback
    std::string lower;
    lower.resize(name.size());
    std::transform(name.begin(), name.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    if (auto a = magic_enum::enum_cast<InputAction>(lower))
      return a;
    // Short aliases
    static const std::map<std::string, InputAction> alias = {
        {"next", InputAction::WidgetNext},   {"back", InputAction::WidgetBack},
        {"press", InputAction::WidgetPress}, {"up", InputAction::ValueUp},
        {"down", InputAction::ValueDown},
    };
    auto it = alias.find(lower);
    if (it != alias.end())
      return it->second;
    return std::nullopt;
  };
  try {
    toml::table tbl = toml::parse_file(path);
    PlaybackConfig cfg;
    if (auto aq = tbl["autoquit"].value<bool>())
      cfg.auto_quit = *aq;
    if (auto dp = tbl["dump_path"].value<std::string>())
      cfg.dump_path = *dp;

    if (auto arr = tbl["step"].as_array()) {
      for (toml::node &node : *arr) {
        if (auto tab = node.as_table()) {
          PlaybackStep st;
          if (auto pn = (*tab)["pressed"]) {
            if (auto pa = pn.as_array()) {
              for (toml::node &v : *pa) {
                if (auto s = v.value<std::string>()) {
                  if (auto action_opt = parse_action(*s))
                    st.pressed.push_back(*action_opt);
                  else
                    log_warn("Unknown action in pressed: {}", *s);
                }
              }
            }
          }
          if (auto hn = (*tab)["held"]) {
            if (auto ha = hn.as_array()) {
              for (toml::node &v : *ha) {
                if (auto s = v.value<std::string>()) {
                  if (auto action_opt = parse_action(*s))
                    st.held.push_back(*action_opt);
                  else
                    log_warn("Unknown action in held: {}", *s);
                }
              }
            }
          }
          cfg.steps.push_back(std::move(st));
        }
      }
    }
    return cfg;
  } catch (const std::exception &e) {
    log_warn("Failed parsing actions TOML {}: {}", path, e.what());
    return std::nullopt;
  }
}

static void dump_ui_tree_json(const std::string &path) {
  using namespace afterhours::ui;
  // Find the entity that owns the AutoLayoutRoot (the UI tree root)
  Entity &root_ent =
      EntityQuery().whereHasComponent<AutoLayoutRoot>().gen_first_enforce();
  std::function<void(EntityID, std::stringstream &, int)> rec;
  rec = [&](EntityID id, std::stringstream &ss, int depth) {
    auto opt = EntityHelper::getEntityForID(id);
    if (!opt) {
      ss << "{\"id\":" << id
         << ",\"name\":\"missing\",\"rect\":{\"x\":0,\"y\":0,\"w\":0,\"h\":0},"
            "\"children\":[]}";
      return;
    }
    Entity &e = opt.asE();
    if (!e.has<UIComponent>()) {
      ss << "{\"id\":" << id
         << ",\"name\":\"no_uicmp\",\"rect\":{\"x\":0,\"y\":0,\"w\":0,\"h\":0},"
            "\"children\":[]}";
      return;
    }
    UIComponent &cmp = e.get<UIComponent>();
    RectangleType r = cmp.rect();
    std::string name = e.has<UIComponentDebug>()
                           ? e.get<UIComponentDebug>().name()
                           : std::string("unknown");
    ss << "{\"id\":" << id << ",\"name\":\"" << name << "\",";
    ss << "\"rect\":{\"x\":" << r.x << ",\"y\":" << r.y << ",\"w\":" << r.width
       << ",\"h\":" << r.height << "},";
    ss << "\"children\":[";
    for (size_t i = 0; i < cmp.children.size(); ++i) {
      rec(cmp.children[i], ss, depth + 1);
      if (i + 1 < cmp.children.size())
        ss << ",";
    }
    ss << "]}";
  };

  std::stringstream ss;
  ss << "{";
  ss << "\"root\":";
  rec(root_ent.id, ss, 0);
  ss << "}";
  std::ofstream out(path);
  if (out)
    out << ss.str();
}

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
    ExampleState &examples = entity.addComponentIfMissing<ExampleState>();

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
    auto nav_el = navigation_bar(
        context, mk(root.ent(), 0), page_names, nav_index,
        ComponentConfig()
            .with_size(ComponentSize{percent(1.f), pixels(50.f)})
            .with_flex_direction(FlexDirection::Row)
            .with_hidden(entity.addComponentIfMissing<ExampleState>().showing)
            .with_debug_name("nav_bar"));
    state.current_page_index = nav_index;
    if (context.has_focus(context.ROOT)) {
      // Focus first child of nav bar so simulated tab/press works
      // deterministically
      auto &cmp = nav_el.ent().get<ui::UIComponent>();
      if (!cmp.children.empty())
        context.set_focus(cmp.children[0]);
    }

    // Main content area
    auto content = div(
        context, mk(root.ent(), 1),
        ComponentConfig()
            .with_size(ComponentSize{percent(1.f), children()})
            .with_flex_direction(FlexDirection::Column)
            .with_hidden(entity.addComponentIfMissing<ExampleState>().showing)
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

      // Button to open deterministic example overlay
      if (button(context, mk(content.ent(), 1),
                 ComponentConfig()
                     .with_label("Open Examples")
                     .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
                     .with_select_on_focus(true)
                     .with_debug_name("open_examples"))) {
        examples.showing = true;
      }

      auto gallery = div(context, mk(content.ent(), 2),
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

      // In playback mode, force examples overlay to ensure deterministic tree
      if (g_playback_config.has_value()) {
        examples.showing = true;
      }

      // Deterministic example overlay (modal)
      if (examples.showing) {
        // Full-screen overlay on top
        auto overlay =
            div(context, mk(root.ent(), 1001),
                ComponentConfig()
                    .with_size(ComponentSize{pixels(1100.f), pixels(650.f)})
                    .with_absolute_position()
                    .with_render_layer(100)
                    .with_color_usage(afterhours::ui::Theme::Usage::Background)
                    .with_debug_name("examples_overlay"));

        // Inner panel
        auto panel = div(
            context, mk(overlay.ent(), 0),
            ComponentConfig()
                .with_size(ComponentSize{pixels(1000.f), pixels(600.f)})
                .with_margin(Margin{.top = pixels(25.f), .left = pixels(50.f)})
                .with_color_usage(afterhours::ui::Theme::Usage::Secondary)
                .with_debug_name("examples_panel"));

        div(context, mk(panel.ent(), 0),
            ComponentConfig()
                .with_label("Example Screen A")
                .with_size(ComponentSize{children(), pixels(50.f)})
                .with_color_usage(afterhours::ui::Theme::Usage::Primary)
                .with_debug_name("example_header"));

        auto body = div(context, mk(panel.ent(), 1),
                        ComponentConfig()
                            .with_size(ComponentSize{children(), pixels(500.f)})
                            .with_flex_direction(FlexDirection::Row)
                            .with_debug_name("example_body"));

        // Left column
        auto col_left =
            div(context, mk(body.ent(), 0),
                ComponentConfig()
                    .with_size(ComponentSize{pixels(480.f), children()})
                    .with_debug_name("example_col_left"));
        button(context, mk(col_left.ent(), 0),
               ComponentConfig()
                   .with_label("Action")
                   .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
                   .with_debug_name("example_action_button"));
        static bool ex_cb = true;
        checkbox(context, mk(col_left.ent(), 1), ex_cb,
                 ComponentConfig()
                     .with_label("Enabled")
                     .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
                     .with_debug_name("example_enabled_checkbox"));

        // Right column
        auto col_right =
            div(context, mk(body.ent(), 1),
                ComponentConfig()
                    .with_size(ComponentSize{pixels(520.f), children()})
                    .with_debug_name("example_col_right"));
        static float ex_slider = 0.75f;
        slider(context, mk(col_right.ent(), 0), ex_slider,
               ComponentConfig()
                   .with_label("Strength")
                   .with_size(ComponentSize{pixels(300.f), pixels(50.f)})
                   .with_debug_name("example_strength_slider"));

        // Close button at bottom
        if (button(context, mk(panel.ent(), 2),
                   ComponentConfig()
                       .with_label("Close")
                       .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
                       .with_debug_name("examples_close"))) {
          examples.showing = false;
        }
      }
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

// Injects test inputs from the playback config each frame
struct ActionPlaybackSystem : System<> {
  size_t current_step = 0;
  bool done = false;

  virtual void for_each_with(Entity &, float dt) override {
    (void)dt;
    if (!g_playback_config.has_value() || done)
      return;
    auto pic = input::get_input_collector<InputAction>();
    if (!pic.has_value())
      return;
    // use accessors directly below, do not bind to avoid unused warnings

    const PlaybackConfig &cfg = g_playback_config.value();
    if (current_step < cfg.steps.size()) {
      const PlaybackStep &step = cfg.steps[current_step];
      for (auto a : step.held) {
        pic.inputs().push_back(afterhours::input::ActionDone<InputAction>{
            .medium = input::DeviceMedium::Keyboard,
            .id = 0,
            .action = a,
            .amount_pressed = 1.f,
            .length_pressed = dt});
      }
      for (auto a : step.pressed) {
        pic.inputs_pressed().push_back(
            afterhours::input::ActionDone<InputAction>{
                .medium = input::DeviceMedium::Keyboard,
                .id = 0,
                .action = a,
                .amount_pressed = 1.f,
                .length_pressed = dt});
      }
      current_step++;
    } else {
      done = true;
      // Dump UI tree if requested and request quit
      dump_ui_tree_json(cfg.dump_path);
      if (cfg.auto_quit)
        g_should_quit = true;
    }
  }
};

int main(int argc, char **argv) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight,
                     "UI Afterhours - Component Showcase");
  raylib::SetTargetFPS(200);

  // Parse CLI args for action playback; fallback to AH_ACTIONS env var
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const std::string prefix = "--actions=";
    if (arg.rfind(prefix, 0) == 0) {
      std::string path = arg.substr(prefix.size());
      auto cfg = load_actions_toml(path);
      if (cfg.has_value()) {
        g_playback_config = cfg;
      } else {
        log_error("Failed to load actions file: {}", path);
      }
    }
  }
  if (!g_playback_config.has_value()) {
    const char *env = std::getenv("AH_ACTIONS");
    if (env && env[0] != '\0') {
      auto cfg = load_actions_toml(env);
      if (cfg.has_value()) {
        g_playback_config = cfg;
      } else {
        log_error("Failed to load actions file: {}", env);
      }
    }
  }

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
    if (g_playback_config.has_value()) {
      systems.register_update_system(std::make_unique<ActionPlaybackSystem>());
    }
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
    if (g_should_quit.load())
      break;
  }

  raylib::CloseWindow();

  return 0;
}
