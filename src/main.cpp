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
#include "toml.hpp"
#include "ui_demo/dump.h"
#include "ui_demo/input_mapping.h"
#include "ui_demo/playback.h"
#include "ui_demo/router.h"
#include "ui_demo/styling.h"

// Workaround for missing log_once_per function - must be defined before
// afterhours includes
#ifndef AFTER_HOURS_REPLACE_LOGGING
inline void log_once_per(...) {
  // Empty implementation to satisfy the linker
}
#endif

//
// These are already set and included by rl.h; avoid redefining them here
// #define AFTER_HOURS_DEBUG
// #define AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
// #define AFTER_HOURS_REPLACE_LOGGING
// #include <afterhours/ah.h>
// #define AFTER_HOURS_USE_RAYLIB
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

// Use types from rl.h (raylib::Vector2/3/4 already typedef'd there if needed)

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

using afterhours::input;

std::optional<PlaybackConfig> g_playback_config;
std::atomic<bool> g_should_quit{false};
// Optional CLI-configured delay between playback steps (in seconds)
float g_step_delay_seconds = 0.0f;

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
    // Note: delay is controlled by CLI, not TOML, to keep tests deterministic

    // Derive scenario name from toml path (basename without extension)
    {
      std::string base = path;
      // strip directory
      size_t slash = base.find_last_of("/\\");
      if (slash != std::string::npos)
        base = base.substr(slash + 1);
      // strip extension
      size_t dot = base.find_last_of('.');
      if (dot != std::string::npos)
        base = base.substr(0, dot);
      cfg.scenario_name = base;
    }

    // Optional button table
    if (auto btn = tbl["button"].as_table()) {
      if (auto hl = (*btn)["hasLabel"].value<bool>()) {
        cfg.button_has_label = *hl;
      }
      if (auto col = (*btn)["color"].value<std::string>()) {
        cfg.button_color = *col;
      }
      if (auto dis = (*btn)["disabled"].value<bool>()) {
        cfg.button_disabled = *dis;
      }
    }

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

// dump_ui_tree_json moved to ui_demo/dump.h

// get_mapping moved to ui_demo/input_mapping.h

// Duplicated in src/ui_demo/styling.h

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

// Duplicated in src/ui_demo/router.h/cpp

// Injects test inputs from the playback config each frame
struct ActionPlaybackSystem : System<> {
  size_t current_step = 0;
  bool done = false;
  float wait_timer = 0.0f;

  virtual void for_each_with(Entity &, float dt) override {
    if (!g_playback_config.has_value() || done)
      return;
    auto pic = input::get_input_collector<InputAction>();
    if (!pic.has_value())
      return;
    // use accessors directly below, do not bind to avoid unused warnings

    const PlaybackConfig &cfg = g_playback_config.value();
    if (current_step < cfg.steps.size()) {
      // If a delay is configured (via CLI), count down before next step
      if (g_step_delay_seconds > 0.0f && wait_timer > 0.0f) {
        wait_timer -= dt;
        return;
      }
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
      // Reset delay timer after applying a step
      if (g_step_delay_seconds > 0.0f) {
        wait_timer = g_step_delay_seconds;
      }
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

  // Pre-parse CLI for headless/no-window mode so we can set flags before
  // InitWindow
  bool start_hidden_window = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-window" || arg == "--headless") {
      start_hidden_window = true;
    }
  }
  if (start_hidden_window) {
    // Hide the window but still initialize raylib so systems depending on it
    // work
    raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
    log_info("Starting in hidden window mode (--no-window)");
  }

  raylib::InitWindow(screenWidth, screenHeight,
                     "UI Afterhours - Component Showcase");
  raylib::SetTargetFPS(200);

  // Parse CLI args for action playback; fallback to AH_ACTIONS env var
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    const std::string prefix = "--actions=";
    const std::string delay_ms_prefix = "--delay=";
    if (arg.rfind(prefix, 0) == 0) {
      std::string path = arg.substr(prefix.size());
      auto cfg = load_actions_toml(path);
      if (cfg.has_value()) {
        g_playback_config = cfg;
      } else {
        log_error("Failed to load actions file: {}", path);
      }
    } else if (arg.rfind(delay_ms_prefix, 0) == 0) {
      const std::string v = arg.substr(delay_ms_prefix.size());
      try {
        int ms = std::stoi(v);
        if (ms < 0)
          ms = 0;
        g_step_delay_seconds = static_cast<float>(ms) / 1000.0f;
      } catch (...) {
        log_warn("Invalid --delay value: '{}'", v);
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
