#include "example_a.h"
#include "afterhours/src/plugins/ui/immediate.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace ui_demo {
namespace examples {

void render_example_a(UIX &context, afterhours::Entity &panel) {
  auto body = div(context, mk(panel, 1),
                  ComponentConfig()
                      .with_size(ComponentSize{children(), pixels(500.f)})
                      .with_flex_direction(FlexDirection::Row)
                      .with_debug_name("example_body"));

  auto col_left = div(context, mk(body.ent(), 0),
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

  auto col_right = div(context, mk(body.ent(), 1),
                       ComponentConfig()
                           .with_size(ComponentSize{pixels(520.f), children()})
                           .with_debug_name("example_col_right"));
  static float ex_slider = 0.75f;
  slider(context, mk(col_right.ent(), 0), ex_slider,
         ComponentConfig()
             .with_label("Strength")
             .with_size(ComponentSize{pixels(300.f), pixels(50.f)})
             .with_debug_name("example_strength_slider"));
}

} // namespace examples
} // namespace ui_demo
