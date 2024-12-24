

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
#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
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
    raylib::DrawFPS((int)(pCurrentResolution.width() - 80), 0);
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

struct HasColor : BaseComponent {
  raylib::Color color;
  HasColor(raylib::Color c) : color(c) {}
};

struct HasChildrenComponent : BaseComponent {
  std::vector<EntityID> children;
  HasChildrenComponent() {}
  void add_child(EntityID child) { children.push_back(child); }
};

struct HasDropdownState : ui::HasCheckboxState {
  using Options = std::vector<std::string>;
  Options options;
  std::function<Options(HasDropdownState &)> fetch_options;
  std::function<void(size_t)> on_option_changed;
  size_t last_option_clicked = 0;

  HasDropdownState(
      const Options &opts,
      const std::function<Options(HasDropdownState &)> fetch_opts = nullptr,
      const std::function<void(size_t)> opt_changed = nullptr)
      : HasCheckboxState(false), options(opts), fetch_options(fetch_opts),
        on_option_changed(opt_changed) {}

  HasDropdownState(const std::function<Options(HasDropdownState &)> fetch_opts)
      : HasDropdownState(fetch_opts(*this), fetch_opts, nullptr) {}
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

  ProviderConsumer() : DataStorage({{}}, get_data_from_provider, nullptr) {}

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

  HasDropdownStateWithProvider() : PC() {}

  virtual HasDropdownState::Options
  convert_from_fetch(PC::return_type::type fetched_data) const override {

    // TODO see message above
    // static_assert(is_vector_data_convertible_to_string<
    // decltype(fetch_data_result)>::value, "ProviderComponent::fetch_data must
    // return a vector of items " "convertible to string");
    HasDropdownState::Options new_options;
    for (auto &data : fetched_data) {
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

struct HandleDrags : SystemWithUIContext<InputAction, ui::HasDragListener> {
  virtual ~HandleDrags() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasDragListener &hasDragListener, float) override {
    if (!context_entity)
      return;
    UIContext<InputAction> &context =
        context_entity->get<UIContext<InputAction>>();

    context.active_if_mouse_inside(entity.id, component.rect());

    if (context.has_focus(entity.id) &&
        context.pressed(InputAction::WidgetPress)) {
      context.set_focus(entity.id);
      hasDragListener.cb(entity);
    }

    if (context.is_active(entity.id)) {
      context.set_focus(entity.id);
      hasDragListener.cb(entity);
    }
  }
};

struct HandleTabbing : SystemWithUIContext<InputAction> {
  virtual ~HandleTabbing() {}

  virtual void for_each_with(Entity &entity, UIComponent &, float) override {
    if (!context_entity)
      return;

    UIContext<InputAction> &context =
        context_entity->get<UIContext<InputAction>>();
    context.try_to_grab(entity.id);
    context.process_tabbing(entity.id);
  }
};

struct RenderUIComponents
    : SystemWithUIContext<InputAction, Transform, HasColor> {
  virtual ~RenderUIComponents() {}
  virtual void for_each_with(const Entity &entity, const UIComponent &,
                             const Transform &transform,
                             const HasColor &hasColor, float) const override {
    if (!context_entity)
      return;
    if (entity.has<ShouldHide>())
      return;
    UIContext<InputAction> &context =
        context_entity->get<UIContext<InputAction>>();

    raylib::Color col = hasColor.color;
    if (context.is_hot(entity.id)) {
      col = raylib::RED;
    }

    if (context.has_focus(entity.id)) {
      raylib::DrawRectangleRec(transform.focus_rect(), raylib::PINK);
    }
    raylib::DrawRectangleV(transform.position, transform.size, col);
    if (entity.has<HasLabel>()) {
      DrawText(entity.get<HasLabel>().label.c_str(), (int)transform.position.x,
               (int)transform.position.y, (int)(transform.size.y / 2.f),
               raylib::RAYWHITE);
    }
  }
};

struct UpdateDropdownOptions
    : SystemWithUIContext<InputAction, HasDropdownState, HasChildrenComponent> {

  UpdateDropdownOptions()
      : SystemWithUIContext<InputAction, HasDropdownState,
                            HasChildrenComponent>() {
    include_derived_children = true;
  }

  void reuse_dropdown_child(UIComponent &component, Entity &entity, size_t i,
                            const std::string &option) {
    Entity &child =
        EQ().whereID(entity.get<ui::HasChildrenComponent>().children[i])
            .gen_first_enforce();

    child.get<UIComponent>()
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.x,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.y,
        })
        .set_parent(entity.id);

    HasDropdownState &hds = entity.get_with_child<HasDropdownState>();
    if (hds.on || hds.last_option_clicked == i) {
      entity.get<UIComponent>().add_child(child.id);
    }
  }

  void make_dropdown_child(UIComponent &component, Entity &entity, size_t i,
                           const std::string &option) {
    Entity &child = EntityHelper::createEntity();

    child.addComponent<UIComponent>(child.id)
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.x,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.y,
        })
        .set_parent(entity.id);

    entity.get<HasChildrenComponent>().add_child(child.id);

    HasDropdownState &hds = entity.get_with_child<HasDropdownState>();
    if (hds.on || hds.last_option_clicked == i) {
      entity.get<UIComponent>().add_child(child.id);
    }
    child.addComponent<ui::HasColor>(raylib::PURPLE);
    child.addComponent<ui::HasLabel>(option);

    child.addComponent<ui::HasClickListener>([i, &entity](Entity &) {
      ui::HasDropdownState &hds = entity.get_with_child<HasDropdownState>();
      if (hds.on_option_changed)
        hds.on_option_changed(i);
      hds.last_option_clicked = i;

      {
        bool nv = !hds.on;
        UIComponent &component = entity.get<UIComponent>();
        for (auto id : entity.get<HasChildrenComponent>().children) {
          if (nv) {
            component.add_child(id);
          } else {
            component.remove_child(id);
          }
        }
        entity.get<ui::HasDropdownState>().on = nv;
      }

      OptEntity opt_context =
          EQ().whereHasComponent<UIContext<InputAction>>().gen_first();
      opt_context->get<ui::UIContext<InputAction>>().set_focus(entity.id);
    });
  }

  virtual void for_each_with_derived(Entity &entity, UIComponent &component,
                                     HasDropdownState &hasDropdownState,
                                     HasChildrenComponent &hasChildren,
                                     float) override {

    // TODO maybe we should fetch only once a second or something?

    const auto options = hasDropdownState.fetch_options(hasDropdownState);
    // delete existing
    component.children.clear();

    for (size_t i = 0; i < options.size(); i++) {
      auto &option = options[i];

      if (hasChildren.children.size() > i) {
        reuse_dropdown_child(component, entity, i, option);
      } else {
        make_dropdown_child(component, entity, i, option);
      }
    }

    hasDropdownState.last_option_clicked =
        (size_t)std::max(std::min((int)options.size() - 1,
                                  (int)hasDropdownState.last_option_clicked),
                         0);
    hasDropdownState.options = options;
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
  background.get<ui::HasChildrenComponent>().add_child(left_padding.id);
  background.get<ui::HasChildrenComponent>().add_child(handle.id);
}

void make_dropdown(
    Entity &parent, vec2 position,
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
  dropdown.addComponent<ui::HasChildrenComponent>();
}

template <typename ProviderComponent> void make_dropdown(vec2 position) {
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
  const auto dropdown_click = [](Entity &entity) {
    bool nv = !entity.get<WRDS>().on;

    UIComponent &component = entity.get<UIComponent>();
    for (auto id : entity.get<HasChildrenComponent>().children) {
      auto opt_child = EQ().whereID(id).gen_first();
      if (nv) {
        component.add_child(id);
      } else {
        component.remove_child(id);
      }
    }

    entity.get<WRDS>().on = nv;

    if (!nv) {
      WRDS &wrds = entity.get<WRDS>();
      wrds.write_value_change_to_provider(wrds);
    }
  };

  auto &dropdown = EntityHelper::createEntity();
  dropdown.addComponent<ui::UIComponent>(dropdown.id);
  dropdown.addComponent<ui::Transform>(position, button_size);
  dropdown.addComponent<ui::HasColor>(raylib::BLUE);
  dropdown.addComponent<WRDS>();
  dropdown.addComponent<ui::HasLabel>();
  dropdown.addComponent<ui::HasChildrenComponent>();
  dropdown.addComponent<ui::HasClickListener>(dropdown_click);

  log_info("has child {}", dropdown.has_child_of<HasDropdownState>());
  log_info("get child {}",
           type_name<decltype(dropdown.get_with_child<HasDropdownState>())>());
}

struct RenderAutoLayoutRoots
    : SystemWithUIContext<InputAction, AutoLayoutRoot> {

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    UIContext<InputAction> &context =
        context_entity->get<UIContext<InputAction>>();

    raylib::Color col = entity.get<HasColor>().color;

    if (context.is_hot(entity.id)) {
      col = raylib::RED;
    }

    if (context.has_focus(entity.id)) {
      raylib::DrawRectangleRec(cmp.focus_rect(), raylib::PINK);
    }
    raylib::DrawRectangleRec(cmp.rect(), col);
    if (entity.has<HasLabel>()) {
      DrawText(entity.get<HasLabel>().label.c_str(), (int)cmp.x(), (int)cmp.y(),
               (int)(cmp.height() / 2.f), raylib::RAYWHITE);
    }
  }

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    if (entity.has<HasColor>()) {
      render_me(entity);
    }

    for (EntityID child : cmp.children) {
      render(AutoLayout::to_ent(child));
    }
  }

  virtual void for_each_with(const Entity &entity, const UIComponent &,
                             const AutoLayoutRoot &, float) const override {
    render(entity);
  }
};

} // namespace ui
} // namespace afterhours

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight, "wm-afterhours");
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

  {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<ui::UIComponent>(entity.id)
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.x,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.y,
        })
        .set_parent(Sophie.id);
    entity.addComponent<ui::HasColor>(raylib::PURPLE);
    Sophie.get<ui::UIComponent>().add_child(entity.id);
  }

  {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<ui::UIComponent>(entity.id)
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = button_size.x,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Percent,
            .value = 0.5f,
        })
        .set_parent(Sophie.id);
    entity.addComponent<ui::HasColor>(raylib::BEIGE);
    Sophie.get<ui::UIComponent>().add_child(entity.id);
  }

  float y = 200;
  float o = 0;
  float s = 75;
  // ui::make_button(Sophie);
  // ui::make_checkbox(Sophie);
  // ui::make_slider(Sophie);

  ui::make_dropdown(Sophie, vec2{200, y + (o++ * s)}, [](auto &) {
    return std::vector<std::string>{{"default", "option1", "option2"}};
  });
  ui::make_dropdown<window_manager::ProvidesAvailableWindowResolutions>(
      vec2{400, y});

  afterhours::ui::AutoLayout::autolayout(Sophie.get<ui::UIComponent>());
  afterhours::ui::AutoLayout::print_tree(Sophie.get<ui::UIComponent>());

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

  systems.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
  {
    systems.register_update_system(
        std::make_unique<ui::UpdateDropdownOptions>());
    systems.register_update_system(std::make_unique<ui::RunAutoLayout>());
    systems.register_update_system(std::make_unique<ui::HandleTabbing>());
    systems.register_update_system(
        std::make_unique<ui::HandleClicks<InputAction>>());
    systems.register_update_system(std::make_unique<ui::HandleDrags>());
  }
  systems.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());

  // renders
  {
    systems.register_render_system(
        [&](float) { raylib::ClearBackground(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<ui::RenderUIComponents>());
    systems.register_render_system(
        std::make_unique<ui::RenderAutoLayoutRoots>());
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
