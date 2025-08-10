#include "afterhours/src/plugins/ui/immediate.h"
#include "examples.h"
#include "rl.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace ui_demo {
namespace examples {

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
  button(context, mk(col_left.ent(), 0),
         ComponentConfig()
             .with_label("Action")
             .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
             .with_debug_name("example_action_button"));
}

} // namespace examples
} // namespace ui_demo
