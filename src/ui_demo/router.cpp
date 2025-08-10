#include "router.h"
#include "afterhours/src/plugins/ui/immediate.h"
#include "afterhours/src/plugins/ui/systems.h"
#include "ui_demo/playback.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

extern std::optional<PlaybackConfig> g_playback_config; // from main.cpp

// Extracted example rendering: overlay + one or more example screens
static void render_home_page(DemoRouter::UIX &context,
                             afterhours::Entity &contentParent,
                             ExampleState &examples) {
  auto content = div(context, mk(contentParent, 0),
                     ComponentConfig()
                         .with_size(ComponentSize{percent(1.f), children()})
                         .with_flex_direction(FlexDirection::Column)
                         .with_debug_name("home_content"));

  div(context, mk(content.ent(), 0),
      ComponentConfig()
          .with_label("Afterhours UI Demo: Use the nav bar to switch pages.")
          .with_size(ComponentSize{children(), pixels(50.f)})
          .with_skip_tabbing(true)
          .with_debug_name("home_intro"));

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

  static bool home_checkbox = false;
  static float home_slider = 0.25f;
  static size_t home_dd = 0;
  const std::vector<std::string> dd_opts = {"Red", "Green", "Blue"};

  button(context, mk(gallery.ent(), 0), ComponentConfig().with_label("Button"));
  checkbox(context, mk(gallery.ent(), 1), home_checkbox,
           ComponentConfig().with_label("Checkbox"));
  slider(context, mk(gallery.ent(), 2), home_slider,
         ComponentConfig().with_label("Slider"));
  dropdown(context, mk(gallery.ent(), 3), dd_opts, home_dd,
           ComponentConfig().with_label("Dropdown"));

  if (g_playback_config.has_value())
    examples.showing = true;
}

// Extracted example rendering: overlay + one or more example screens
static void render_example_screen_a_body(DemoRouter::UIX &context,
                                         afterhours::Entity &panel) {
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

static void render_examples_overlay(DemoRouter::UIX &context,
                                    afterhours::Entity &rootEntity,
                                    ExampleState &examples) {
  auto overlay =
      div(context, mk(rootEntity, 1001),
          ComponentConfig()
              .with_size(ComponentSize{pixels(1100.f), pixels(650.f)})
              .with_absolute_position()
              .with_color_usage(Theme::Usage::Background)
              .with_debug_name("examples_overlay"));

  auto panel =
      div(context, mk(overlay.ent(), 0),
          ComponentConfig()
              .with_size(ComponentSize{pixels(1000.f), pixels(600.f)})
              .with_margin(Margin{.top = pixels(25.f), .left = pixels(50.f)})
              .with_color_usage(Theme::Usage::Secondary)
              .with_debug_name("examples_panel"));

  div(context, mk(panel.ent(), 0),
      ComponentConfig()
          .with_label("Example Screen A")
          .with_size(ComponentSize{children(), pixels(50.f)})
          .with_color_usage(Theme::Usage::Primary)
          .with_debug_name("example_header"));

  // Body of current example screen
  render_example_screen_a_body(context, panel.ent());

  if (button(context, mk(panel.ent(), 2),
             ComponentConfig()
                 .with_label("Close")
                 .with_size(ComponentSize{pixels(220.f), pixels(50.f)})
                 .with_debug_name("examples_close"))) {
    examples.showing = false;
  }
}

void DemoRouter::for_each_with(afterhours::Entity &entity, UIX &context,
                               float) {
  DemoState &state = entity.addComponentIfMissing<DemoState>();
  ExampleState &examples = entity.addComponentIfMissing<ExampleState>();

  static const std::vector<std::string> page_names = {"Home", "Buttons",
                                                      "Layout"};

  auto root = div(context, mk(entity, 1000),
                  ComponentConfig()
                      .with_size(ComponentSize{pixels(1100.f), pixels(650.f)})
                      .with_flex_direction(FlexDirection::Column)
                      .with_debug_name("demo_root"));

  if (!examples.showing) {
    navigation_bar(context, mk(root.ent(), 0), page_names,
                   state.current_page_index,
                   ComponentConfig()
                       .with_size(ComponentSize{percent(1.f), pixels(50.f)})
                       .with_flex_direction(FlexDirection::Row)
                       .with_debug_name("nav_bar"));

    auto content = div(context, mk(root.ent(), 1),
                       ComponentConfig()
                           .with_size(ComponentSize{percent(1.f), children()})
                           .with_flex_direction(FlexDirection::Column)
                           .with_debug_name("content"));

    switch (state.current_page_index) {
    case 0: {
      render_home_page(context, content.ent(), examples);
      break;
    }
    default:
      break;
    }
  }

  // Render examples overlay on top when active (independent of main content)
  if (examples.showing) {
    render_examples_overlay(context, root.ent(), examples);
  }
}
