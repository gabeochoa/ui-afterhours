#include "afterhours/src/plugins/ui/immediate.h"
#include "examples.h"
#include "ui_demo/playback.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace ui_demo {
namespace examples {

static Theme::Usage
parse_theme_usage_or_default(const std::optional<std::string> &s,
                             Theme::Usage def = Theme::Usage::Primary) {
  if (!s.has_value())
    return def;
  const std::string &v = *s;
  if (v == "Primary")
    return Theme::Usage::Primary;
  if (v == "Secondary")
    return Theme::Usage::Secondary;
  if (v == "Accent")
    return Theme::Usage::Accent;
  if (v == "Error")
    return Theme::Usage::Error;
  if (v == "Background")
    return Theme::Usage::Background;
  return def;
}

void render_single_button(UIX &context, afterhours::Entity &panel) {
  auto body = div(context, mk(panel, 1),
                  ComponentConfig()
                      .with_size(ComponentSize{children(), pixels(500.f)})
                      .with_flex_direction(FlexDirection::Row)
                      .with_debug_name("example_body"));

  auto col_left = div(context, mk(body.ent(), 0),
                      ComponentConfig()
                          .with_size(ComponentSize{pixels(480.f), children()})
                          .with_debug_name("example_col_left"));

  // Determine variants from playback config if present
  bool has_label = true;
  bool disabled = false;
  Theme::Usage usage = Theme::Usage::Primary;
  if (::g_playback_config.has_value()) {
    const PlaybackConfig &cfg = *::g_playback_config;
    if (cfg.button_has_label.has_value())
      has_label = *cfg.button_has_label;
    if (cfg.button_disabled.has_value())
      disabled = *cfg.button_disabled;
    usage =
        parse_theme_usage_or_default(cfg.button_color, Theme::Usage::Primary);
  }

  auto btn_cfg = ComponentConfig()
                     .with_label(has_label ? "Action" : "")
                     .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
                     .with_color_usage(usage)
                     .with_disabled(disabled)
                     .with_debug_name("example_action_button");

  button(context, mk(col_left.ent(), 0), btn_cfg);

  // Add right column to satisfy existing scenario tree expectations if present
  auto col_right = div(context, mk(body.ent(), 1),
                       ComponentConfig()
                           .with_size(ComponentSize{pixels(480.f), children()})
                           .with_debug_name("example_col_right"));

  static bool enabled_checkbox = true;
  checkbox(context, mk(col_left.ent(), 1), enabled_checkbox,
           ComponentConfig()
               .with_label("example_enabled_checkbox")
               .with_debug_name("example_enabled_checkbox"));

  static float strength = 0.5f;
  slider(context, mk(col_right.ent(), 0), strength,
         ComponentConfig()
             .with_label("example_strength_slider")
             .with_debug_name("example_strength_slider"));
}

} // namespace examples
} // namespace ui_demo
