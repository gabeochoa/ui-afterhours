

#include "backward/backward.hpp"

namespace backward {
backward::SignalHandling sh;
} // namespace backward

#include "rl.h"

#include "std_include.h"
#include <cassert>
//
#include "log/log.h"
//
#define AFTER_HOURS_DEBUG
#define AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
#define AFTER_HOURS_REPLACE_LOGGING
#include "afterhours/ah.h"
#define AFTER_HOURS_USE_RAYLIB
#include "afterhours/src/developer.h"
#include "afterhours/src/plugins/input_system.h"
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
#define Vector2Type vec2
#include "afterhours/src/plugins/autolayout.h"
#include "afterhours/src/plugins/ui.h"

namespace myutil {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace myutil

struct EQ : public EntityQuery<EQ> {};

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

const vec2 button_size = vec2{100, 50};

enum class InputAction {
  None,
  WidgetNext,
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

namespace afterhours {
namespace ui {

struct Transform : BaseComponent {
  vec2 position;
  vec2 size;
  Transform(vec2 pos, vec2 sz) : position(pos), size(sz) {}

  raylib::Rectangle rect() const {
    return raylib::Rectangle{position.x, position.y, size.x, size.y};
  }

  raylib::Rectangle focus_rect(int rw = 2) const {
    return raylib::Rectangle{position.x - (float)rw, position.y - (float)rw,
                             size.x + (2.f * (float)rw),
                             size.y + (2.f * (float)rw)};
  }
};

// TODO i really wanted to statically validate this
// so that any developer would get a reasonable compiler msg
// but i couldnt get it working with both trivial types and struct types
// https://godbolt.org/z/7v4n7s1Kn
/*
template <typename T, typename = void>
struct is_vector_data_convertible_to_string : std::false_type {};
template <typename T>
using stringable = std::is_convertible<T, std::string>;

template <typename T>
struct is_vector_data_convertible_to_string;

template <typename T>
struct is_vector_data_convertible_to_string<std::vector<T>> : stringable<T> {};

template <typename T>
struct is_vector_data_convertible_to_string<
    T, std::void_t<decltype(std::to_string(
           std::declval<typename T::value_type>()))>> : std::true_type {};
*/

template <typename Derived, typename DataStorage, typename ProvidedType,
          typename ProviderComponent>
struct ProviderConsumer : public DataStorage {

  struct has_fetch_data_member {
    template <typename U>
    static auto test(U *)
        -> decltype(std::declval<U>().fetch_data(), void(), std::true_type{});

    template <typename U> static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<ProviderComponent>(0))::value;
  };

  struct has_on_data_changed_member {
    template <typename U>
    static auto test(U *) -> decltype(std::declval<U>().on_data_changed(0),
                                      void(), std::true_type{});

    template <typename U> static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<ProviderComponent>(0))::value;
  };

  struct return_type {
    using type = decltype(std::declval<ProviderComponent>().fetch_data());
  };

  ProviderConsumer(const std::function<void(size_t)> &on_change)
      : DataStorage({{}}, get_data_from_provider, on_change) {}

  virtual void write_value_change_to_provider(const DataStorage &storage) = 0;
  virtual ProvidedType convert_from_fetch(return_type::type data) const = 0;

  static ProvidedType get_data_from_provider(const DataStorage &storage) {
    // log_info("getting data from provider");
    static_assert(std::is_base_of_v<BaseComponent, ProviderComponent>,
                  "ProviderComponent must be a child of BaseComponent");
    // Convert the data in the provider component to a list of options
    auto &provider_entity =
        EQ().whereHasComponent<ProviderComponent>().gen_first_enforce();
    ProviderComponent &pComp =
        provider_entity.template get<ProviderComponent>();

    static_assert(has_fetch_data_member::value,
                  "ProviderComponent must have fetch_data function");

    auto fetch_data_result = pComp.fetch_data();
    return static_cast<const Derived &>(storage).convert_from_fetch(
        fetch_data_result);
  }
};

template <typename ProviderComponent>
struct HasDropdownStateWithProvider
    : public ProviderConsumer<HasDropdownStateWithProvider<ProviderComponent>,
                              HasDropdownState, HasDropdownState::Options,
                              ProviderComponent> {

  using PC = ProviderConsumer<HasDropdownStateWithProvider<ProviderComponent>,
                              HasDropdownState, HasDropdownState::Options,
                              ProviderComponent>;

  int max_num_options = -1;
  HasDropdownStateWithProvider(const std::function<void(size_t)> &on_change,
                               int max_options_ = -1)
      : PC(on_change), max_num_options(max_options_) {}

  virtual HasDropdownState::Options
  convert_from_fetch(PC::return_type::type fetched_data) const override {

    // TODO see message above
    // static_assert(is_vector_data_convertible_to_string<
    // decltype(fetch_data_result)>::value, "ProviderComponent::fetch_data must
    // return a vector of items " "convertible to string");
    HasDropdownState::Options new_options;
    for (auto &data : fetched_data) {
      if (max_num_options > 0 && (int)new_options.size() > max_num_options)
        continue;
      new_options.push_back((std::string)data);
    }
    return new_options;
  }

  virtual void
  write_value_change_to_provider(const HasDropdownState &hdswp) override {
    size_t index = hdswp.last_option_clicked;

    auto &entity =
        EQ().whereHasComponent<ProviderComponent>().gen_first_enforce();
    ProviderComponent &pComp = entity.template get<ProviderComponent>();

    static_assert(
        PC::has_on_data_changed_member::value,
        "ProviderComponent must have on_data_changed(index) function");
    pComp.on_data_changed(index);
  }
};

void make_button(Entity &parent) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<UIComponent>(entity.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(entity.id);

  entity.addComponent<ui::HasColor>(raylib::BLUE);
  entity.addComponent<ui::HasLabel>(raylib::TextFormat("button%i", entity.id));
  entity.addComponent<ui::HasClickListener>(
      [](Entity &button) { log_info("I clicked the button {}", button.id); });
}

void make_checkbox(Entity &parent) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<UIComponent>(entity.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(entity.id);

  entity.addComponent<ui::HasColor>(raylib::BLUE);
  entity.addComponent<ui::HasCheckboxState>(false);
  entity.addComponent<ui::HasLabel>(" ");
  entity.addComponent<ui::HasClickListener>([](Entity &checkbox) {
    log_info("I clicked the checkbox {}", checkbox.id);
    ui::HasCheckboxState &hcs = checkbox.get<ui::HasCheckboxState>();
    hcs.on = !hcs.on;
    checkbox.get<ui::HasLabel>().label = fmt::format("{}", hcs.on ? "X" : " ");
  });
}

void make_slider(Entity &parent) {
  // TODO add vertical slider

  auto &background = EntityHelper::createEntity();
  background.addComponent<UIComponent>(background.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(background.id);

  background.addComponent<ui::HasSliderState>(0.5f);
  background.addComponent<ui::HasColor>(raylib::GREEN);
  background.addComponent<ui::HasDragListener>([](Entity &entity) {
    float mnf = 0.f;
    float mxf = 1.f;

    UIComponent &cmp = entity.get<UIComponent>();
    raylib::Rectangle rect = cmp.rect();
    HasSliderState &sliderState = entity.get<ui::HasSliderState>();
    float &value = sliderState.value;

    auto mouse_position = input::get_mouse_position();
    float v = (mouse_position.x - rect.x) / rect.width;
    if (v < mnf)
      v = mnf;
    if (v > mxf)
      v = mxf;
    if (v != value) {
      value = v;
      sliderState.changed_since = true;
    }

    auto opt_child =
        EQ().whereID(entity.get<ui::HasChildrenComponent>().children[0])
            .gen_first();

    UIComponent &child_cmp = opt_child->get<UIComponent>();
    child_cmp.set_desired_width(
        ui::Size{.dim = ui::Dim::Percent, .value = value * 0.75f});

    // log_info("I clicked the slider {} {}", entity.id, value);
  });

  // TODO replace when we have actual padding later
  auto &left_padding = EntityHelper::createEntity();
  left_padding.addComponent<UIComponent>(left_padding.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Percent,
          .value = 0.f,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(background.id);
  background.get<ui::UIComponent>().add_child(left_padding.id);

  auto &handle = EntityHelper::createEntity();
  handle.addComponent<UIComponent>(handle.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x * 0.25f,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(background.id);
  background.get<ui::UIComponent>().add_child(handle.id);

  handle.addComponent<ui::HasColor>(raylib::BLUE);

  background.addComponent<ui::HasChildrenComponent>();
  background.get<ui::HasChildrenComponent>().add_child(left_padding);
  background.get<ui::HasChildrenComponent>().add_child(handle);
}

void make_dropdown(
    Entity &parent,
    const std::function<ui::HasDropdownState::Options(HasDropdownState &)> &fn,
    const std::function<void(size_t)> &on_change = nullptr) {

  auto &dropdown = EntityHelper::createEntity();

  dropdown.addComponent<UIComponent>(dropdown.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Children,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(dropdown.id);

  dropdown.addComponent<ui::HasColor>(raylib::BLUE);
  dropdown.addComponent<ui::HasDropdownState>(ui::HasDropdownState::Options{},
                                              fn, on_change);
  dropdown.addComponent<ui::HasChildrenComponent>().register_on_child_add(
      [](Entity &child) {
        if (child.is_missing<HasColor>()) {
          child.addComponent<HasColor>(raylib::PURPLE);
        }
      });
}

template <typename ProviderComponent>
void make_dropdown(Entity &parent, int max_options = -1) {
  using WRDS = ui::HasDropdownStateWithProvider<ProviderComponent>;

  /*
   * I would like to make something like this,
   * but again this break the System<HasClicklistener>
   * thing
template <typename ProviderComponent>
struct HasDropdownClickListener : HasClickListener {
    using WRDS = ui::HasDropdownStateWithProvider<ProviderComponent>;

  HasDropdownClickListener()
      : HasClickListener([](Entity &entity) { dropdown_click(entity); }) {}
};

*/
  auto &dropdown = EntityHelper::createEntity();
  dropdown.addComponent<UIComponent>(dropdown.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Children,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(dropdown.id);

  dropdown.addComponent<ui::HasColor>(raylib::BLUE);
  dropdown.addComponent<ui::HasChildrenComponent>().register_on_child_add(
      [](Entity &child) {
        if (child.is_missing<HasColor>()) {
          child.addComponent<HasColor>(raylib::PURPLE);
        }
      });

  dropdown.addComponent<WRDS>(
      [&dropdown](size_t) {
        WRDS &wrds = dropdown.get<WRDS>();
        if (!wrds.on) {
          wrds.write_value_change_to_provider(wrds);
        }
      },
      max_options);
}

} // namespace ui
} // namespace afterhours

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight, "ui-afterhours");
  raylib::SetTargetFPS(200);

  // sophie
  auto &Sophie = EntityHelper::createEntity();
  {
    input::add_singleton_components<InputAction>(Sophie, get_mapping());
    window_manager::add_singleton_components(Sophie, 200);
    Sophie.addComponent<ui::UIContext<InputAction>>();

    // making a root component to attach the UI to
    Sophie.addComponent<ui::AutoLayoutRoot>();
    Sophie.addComponent<ui::UIComponent>(Sophie.id)
        .set_desired_width(ui::Size{
            // TODO figure out how to update this
            // when resolution changes
            .dim = ui::Dim::Pixels,
            .value = screenWidth,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = screenHeight,
        });
  }

  ui::make_dropdown<window_manager::ProvidesAvailableWindowResolutions>(Sophie);

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
    ui::register_update_systems<InputAction>(systems);
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
