

#include "backward/backward.hpp"

namespace backward {
backward::SignalHandling sh;
} // namespace backward

#include "rl.h"

#include "std_include.h"
//
#include "log/log.h"
//
#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "afterhours/ah.h"
#define AFTER_HOURS_USE_RAYLIB
#include "afterhours/src/plugins/developer.h"
#include "afterhours/src/plugins/input_system.h"
#include "afterhours/src/plugins/window_manager.h"
#include <cassert>

//
using namespace afterhours;

typedef raylib::Vector2 vec2;
typedef raylib::Vector3 vec3;
typedef raylib::Vector4 vec4;

constexpr float distance_sq(const vec2 a, const vec2 b) {
  return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

namespace myutil {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace myutil

// TODO i kinda dont like that you have to do this even if you dont want to
// customize it
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

using afterhours::input::InputCollector;

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

namespace ui {
inline bool is_mouse_inside(const input::MousePosition &mouse_pos,
                            const raylib::Rectangle &rect) {
  return mouse_pos.x >= rect.x && mouse_pos.x <= rect.x + rect.width &&
         mouse_pos.y >= rect.y && mouse_pos.y <= rect.y + rect.height;
}

struct UIComponent : BaseComponent {};
struct UIContext : BaseComponent {
  EntityID ROOT = -1;
  EntityID FAKE = -2;

  std::set<EntityID> focused_ids;

  EntityID hot_id = ROOT;
  EntityID focus_id = ROOT;
  EntityID active_id = ROOT;
  EntityID last_processed = ROOT;

  input::MousePosition mouse_pos;
  bool mouseLeftDown;
  InputAction last_action;
  std::bitset<magic_enum::enum_count<InputAction>()> all_actions;

  [[nodiscard]] bool is_hot(EntityID id) { return hot_id == id; };
  [[nodiscard]] bool is_active(EntityID id) { return active_id == id; };
  void set_hot(EntityID id) { hot_id = id; }
  void set_active(EntityID id) { active_id = id; }

  bool has_focus(EntityID id) { return focus_id == id; }
  void set_focus(EntityID id) { focus_id = id; }

  void active_if_mouse_inside(EntityID id, raylib::Rectangle rect) {
    if (is_mouse_inside(mouse_pos, rect)) {
      set_hot(id);
      if (is_active(ROOT) && mouseLeftDown) {
        set_active(id);
      }
    }
  }

  void reset() {
    focus_id = ROOT;
    last_processed = ROOT;
    hot_id = ROOT;
    active_id = ROOT;
    focused_ids.clear();
  }

  void try_to_grab(EntityID id) {
    focused_ids.insert(id);
    if (has_focus(ROOT)) {
      set_focus(id);
    }
  }

  [[nodiscard]] bool is_mouse_click(EntityID id) {
    bool let_go = !mouseLeftDown;
    bool was_click = let_go && is_active(id) && is_hot(id);
    // if(was_click){play_sound();}
    return was_click;
  }

  [[nodiscard]] bool pressed(const InputAction &name) {
    bool a = last_action == name;
    if (a) {
      // ui::sounds::select();
      last_action = InputAction::None;
    }
    return a;
  }

  [[nodiscard]] bool is_held_down(const InputAction &name) {
    bool a = all_actions[magic_enum::enum_index<InputAction>(name).value()];
    if (a) {
      // ui::sounds::select();
      all_actions[magic_enum::enum_index<InputAction>(name).value()] = false;
    }
    return a;
  }

  void process_tabbing(EntityID id) {
    // TODO How do we handle something that wants to use
    // Widget Value Down/Up to control the value?
    // Do we mark the widget type with "nextable"? (tab will always work but
    // not very discoverable

    if (has_focus(id)) {
      if (
          //
          pressed(InputAction::WidgetNext) || pressed(InputAction::ValueDown)
          // TODO add support for holding down tab
          // get().is_held_down_debounced(InputAction::WidgetNext) ||
          // get().is_held_down_debounced(InputAction::ValueDown)
      ) {
        set_focus(ROOT);
        if (is_held_down(InputAction::WidgetMod)) {
          set_focus(last_processed);
        }
      }
      if (pressed(InputAction::ValueUp)) {
        set_focus(last_processed);
      }
      // if (pressed(InputAction::WidgetBack)) {
      // set_focus(last_processed);
      // }
    }
    // before any returns
    last_processed = id;
  }
};

struct BeginUIContextManager : System<UIContext> {

  // TODO this should live inside input_system
  // but then it would require magic_enum as a dependency
  std::bitset<magic_enum::enum_count<InputAction>()> inputs_as_bits(
      const std::vector<input::ActionDone<InputAction>> &inputs) const {
    std::bitset<magic_enum::enum_count<InputAction>()> output;
    for (auto &input : inputs) {
      if (input.amount_pressed <= 0.f)
        continue;
      output[magic_enum::enum_index<InputAction>(input.action).value()] = true;
    }
    return output;
  }

  virtual void for_each_with(Entity &, UIContext &context, float) override {
    context.mouse_pos = input::get_mouse_position();
    context.mouseLeftDown = input::is_mouse_button_down(0);

    {
      input::PossibleInputCollector<InputAction> inpc =
          input::get_input_collector<InputAction>();
      if (inpc.has_value()) {
        context.all_actions = inputs_as_bits(inpc.inputs());
        for (auto &actions_done : inpc.inputs_pressed()) {
          context.last_action = actions_done.action;
        }
      }
    }

    context.hot_id = context.ROOT;
  }
};

struct EndUIContextManager : System<UIContext> {

  virtual void for_each_with(Entity &, UIContext &context, float) override {

    if (context.focus_id == context.ROOT)
      return;

    if (context.mouseLeftDown) {
      if (context.is_active(context.ROOT)) {
        context.set_active(context.FAKE);
      }
    } else {
      context.set_active(context.ROOT);
    }
    if (!context.focused_ids.contains(context.focus_id))
      context.focus_id = context.ROOT;
    context.focused_ids.clear();
  }
};

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

struct HasClickListener : BaseComponent {
  bool down = false;
  std::function<void(Entity &)> cb;
  HasClickListener(const std::function<void(Entity &)> &callback)
      : cb(callback) {}
};

struct HasDragListener : BaseComponent {
  bool down = false;
  std::function<void(Entity &)> cb;
  HasDragListener(const std::function<void(Entity &)> &callback)
      : cb(callback) {}
};

struct HasLabel : BaseComponent {
  std::string label;
  HasLabel(const std::string &str) : label(str) {}
  HasLabel() : label("") {}
};

struct HasCheckboxState : BaseComponent {
  bool on;
  HasCheckboxState(bool b) : on(b) {}
};

struct HasSliderState : BaseComponent {
  bool changed_since = false;
  float value;
  HasSliderState(float val) : value(val) {}
};

struct HasChildrenComponent : BaseComponent {
  std::vector<EntityID> children;
  HasChildrenComponent() {}
  void add_child(EntityID child) { children.push_back(child); }
};

struct ShouldHide : BaseComponent {};

struct HasDropdownState : HasCheckboxState {
  using Options = std::vector<std::string>;
  Options options;
  std::function<Options(void)> fetch_options;
  std::function<void(size_t)> on_option_changed;
  size_t last_option_clicked = 0;

  HasDropdownState(const Options &opts,
                   const std::function<Options(void)> fetch_opts = nullptr,
                   const std::function<void(size_t)> opt_changed = nullptr)
      : HasCheckboxState(false), options(opts), fetch_options(fetch_opts),
        on_option_changed(opt_changed) {}

  HasDropdownState(const std::function<Options(void)> fetch_opts)
      : HasDropdownState(fetch_opts(), fetch_opts, nullptr) {}
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

template <typename T> struct has_fetch_data_member {
  template <typename U>
  static auto test(U *)
      -> decltype(std::declval<U>().fetch_data(), void(), std::true_type{});

  template <typename U> static auto test(...) -> std::false_type;

  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> struct has_on_data_changed_member {
  template <typename U>
  static auto test(U *) -> decltype(std::declval<U>().on_data_changed(0),
                                    void(), std::true_type{});

  template <typename U> static auto test(...) -> std::false_type;

  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> struct return_type {
  using type = decltype(std::declval<T>().fetch_data());
};

template <typename ProviderComponent>
struct HasDropdownStateWithProvider : HasDropdownState {
  return_type<ProviderComponent> provided_data;

  static void
  write_value_change_to_provider(const HasDropdownStateWithProvider &hdswp) {
    size_t index = hdswp.last_option_clicked;

    auto &entity =
        EQ().whereHasComponent<ProviderComponent>().gen_first_enforce();
    ProviderComponent &pComp = entity.template get<ProviderComponent>();

    static_assert(
        has_on_data_changed_member<ProviderComponent>::value,
        "ProviderComponent must have on_data_changed(index) function");
    pComp.on_data_changed(index);
  }

  static Options get_data_from_provider() {
    // log_info("getting data from provider");
    static_assert(std::is_base_of_v<BaseComponent, ProviderComponent>,
                  "ProviderComponent must be a child of BaseComponent");
    // Convert the data in the provider component to a list of options
    auto &entity =
        EQ().whereHasComponent<ProviderComponent>().gen_first_enforce();
    ProviderComponent &pComp = entity.template get<ProviderComponent>();

    static_assert(has_fetch_data_member<ProviderComponent>::value,
                  "ProviderComponent must have fetch_data function");

    auto fetch_data_result = pComp.fetch_data();

    // TODO see message above
    // static_assert(is_vector_data_convertible_to_string<
    // decltype(fetch_data_result)>::value,
    // "ProviderComponent::fetch_data must return a vector of items "
    // "convertible to string");

    Options new_options;
    for (auto &data : fetch_data_result) {
      new_options.push_back((std::string)data);
    }
    return new_options;
  }

  HasDropdownStateWithProvider()
      : HasDropdownState({{}}, get_data_from_provider, nullptr) {}
};

// TODO i like this but for Tags, i wish
// the user of this didnt have to add UIComponent to their for_each_with
template <typename... Components>
struct SystemWithUIContext : System<UIComponent, Components...> {
  Entity *context_entity;
  virtual void once(float) override {
    OptEntity opt_context = EQ().whereHasComponent<UIContext>().gen_first();
    context_entity = opt_context.value();
  }
};

struct HandleClicks : SystemWithUIContext<Transform, HasClickListener> {
  virtual ~HandleClicks() {}

  virtual void for_each_with(Entity &entity, UIComponent &,
                             Transform &transform,
                             HasClickListener &hasClickListener,
                             float) override {
    if (!context_entity)
      return;
    if (entity.has<ShouldHide>())
      return;
    UIContext &context = context_entity->get<UIContext>();

    context.active_if_mouse_inside(entity.id, transform.rect());

    if (context.has_focus(entity.id) &&
        context.pressed(InputAction::WidgetPress)) {
      context.set_focus(entity.id);
      hasClickListener.cb(entity);
    }

    if (context.is_mouse_click(entity.id)) {
      context.set_focus(entity.id);
      hasClickListener.cb(entity);
    }
  }
};

struct HandleDrags : SystemWithUIContext<Transform, HasDragListener> {
  virtual ~HandleDrags() {}

  virtual void for_each_with(Entity &entity, UIComponent &,
                             Transform &transform,
                             HasDragListener &hasDragListener, float) override {
    if (!context_entity)
      return;
    UIContext &context = context_entity->get<UIContext>();

    context.active_if_mouse_inside(entity.id, transform.rect());

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

struct HandleTabbing : SystemWithUIContext<> {
  virtual ~HandleTabbing() {}

  virtual void for_each_with(Entity &entity, UIComponent &, float) override {
    if (!context_entity)
      return;
    if (entity.has<ShouldHide>())
      return;

    UIContext &context = context_entity->get<UIContext>();
    context.try_to_grab(entity.id);
    context.process_tabbing(entity.id);
  }
};

struct RenderUIComponents : SystemWithUIContext<Transform, HasColor> {
  virtual ~RenderUIComponents() {}
  virtual void for_each_with(const Entity &entity, const UIComponent &,
                             const Transform &transform,
                             const HasColor &hasColor, float) const override {
    if (!context_entity)
      return;
    if (entity.has<ShouldHide>())
      return;
    UIContext &context = context_entity->get<UIContext>();

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
    : SystemWithUIContext<Transform, HasDropdownState, HasChildrenComponent> {

  UpdateDropdownOptions()
      : SystemWithUIContext<Transform, HasDropdownState,
                            HasChildrenComponent>() {
    include_derived_children = true;
  }

  void make_dropdown_child(Transform &transform, Entity &entity, size_t i,
                           const std::string &option) {
    auto &child = EntityHelper::createEntity();
    entity.get<HasChildrenComponent>().add_child(child.id);
    child.addComponent<ui::UIComponent>();
    child.addComponent<ui::Transform>(
        transform.position + vec2{0.f, button_size.y * ((float)i + 1.f)},
        button_size);
    child.addComponent<ui::HasColor>(raylib::PURPLE);
    child.addComponent<ui::HasLabel>(option);
    child.addComponent<ui::ShouldHide>();

    child.addComponent<ui::HasClickListener>([i, &entity](Entity &) {
      ui::HasDropdownState &hds = entity.get_with_child<HasDropdownState>();
      if (hds.on_option_changed)
        hds.on_option_changed(i);
      hds.last_option_clicked = i;
      entity.get<ui::HasClickListener>().cb(entity);

      OptEntity opt_context = EQ().whereHasComponent<UIContext>().gen_first();
      opt_context->get<ui::UIContext>().set_focus(entity.id);

      entity.get<HasLabel>().label = hds.options[hds.last_option_clicked];
    });
  }

  virtual void for_each_with_derived(Entity &entity, UIComponent &,
                                     Transform &transform,
                                     HasDropdownState &hasDropdownState,
                                     HasChildrenComponent &hasChildren,
                                     float) override {

    // TODO maybe we should fetch only once a second or something?
    const auto options = hasDropdownState.fetch_options();

    if (options.size() == hasDropdownState.options.size()) {
      bool any_changed = false;
      for (size_t i = 0; i < options.size(); i++) {
        auto option = options[i];
        auto old_option = hasDropdownState.options[i];
        if (option != old_option) {
          any_changed = true;
          break;
        }
      }

      // no need to regenerate UI
      if (!any_changed)
        return;
    }
    // update the children

    // delete existing
    {
      for (auto &child_id : hasChildren.children) {
        EntityHelper::markIDForCleanup(child_id);
      }
      hasChildren.children.clear();
    }

    for (size_t i = 0; i < options.size(); i++) {
      auto &option = options[i];
      make_dropdown_child(transform, entity, i, option);
    }

    hasDropdownState.last_option_clicked =
        (size_t)std::max(std::min((int)options.size() - 1,
                                  (int)hasDropdownState.last_option_clicked),
                         0);
    entity.get<HasLabel>().label =
        options[hasDropdownState.last_option_clicked];

    hasDropdownState.options = options;
  }
};

void make_button(vec2 position) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<ui::UIComponent>();
  entity.addComponent<ui::Transform>(position, button_size);
  entity.addComponent<ui::HasColor>(raylib::BLUE);
  entity.addComponent<ui::HasLabel>(raylib::TextFormat("button%i", entity.id));
  entity.addComponent<ui::HasClickListener>(
      [](Entity &button) { log_info("I clicked the button {}", button.id); });
}

void make_checkbox(vec2 position) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<ui::UIComponent>();
  entity.addComponent<ui::Transform>(position, button_size);
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

void make_slider(vec2 position) {
  // TODO add vertical slider

  auto &background = EntityHelper::createEntity();
  background.addComponent<ui::UIComponent>();
  background.addComponent<ui::Transform>(position,
                                         vec2{button_size.x, button_size.y});
  background.addComponent<ui::HasSliderState>(0.5f);
  background.addComponent<ui::HasColor>(raylib::GREEN);

  background.addComponent<ui::HasDragListener>([](Entity &entity) {
    float mnf = 0.f;
    float mxf = 1.f;

    Transform &transform = entity.get<Transform>();
    raylib::Rectangle rect = transform.rect();
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

    float position_offset = value * rect.width * 0.75f;

    auto opt_child =
        EQ().whereID(entity.get<ui::HasChildrenComponent>().children[0])
            .gen_first();

    Transform &child_transform = opt_child->get<Transform>();
    child_transform.position = {rect.x + position_offset, rect.y};

    log_info("I clicked the slider {} {}", entity.id, value);
  });

  auto &handle = EntityHelper::createEntity();
  handle.addComponent<ui::UIComponent>();
  handle.addComponent<ui::Transform>(
      position, vec2{button_size.x * 0.25f, button_size.y});
  handle.addComponent<ui::HasColor>(raylib::BLUE);

  background.addComponent<ui::HasChildrenComponent>();
  background.get<ui::HasChildrenComponent>().add_child(handle.id);
}

void make_dropdown(vec2 position,
                   const std::function<ui::HasDropdownState::Options(void)> &fn,
                   const std::function<void(size_t)> &on_change = nullptr) {
  auto &dropdown = EntityHelper::createEntity();
  dropdown.addComponent<ui::UIComponent>();
  dropdown.addComponent<ui::Transform>(position, button_size);
  dropdown.addComponent<ui::HasColor>(raylib::BLUE);
  dropdown.addComponent<ui::HasDropdownState>(ui::HasDropdownState::Options{},
                                              fn, on_change);
  dropdown.addComponent<ui::HasLabel>();
  dropdown.addComponent<ui::HasChildrenComponent>();
  dropdown.addComponent<ui::HasClickListener>([](Entity &entity) {
    bool nv = !entity.get<ui::HasDropdownState>().on;
    log_info("dropdown {}", nv);

    for (auto id : entity.get<HasChildrenComponent>().children) {
      auto opt_child = EQ().whereID(id).gen_first();
      if (nv) {
        opt_child->removeComponent<ShouldHide>();
      } else {
        opt_child->addComponent<ShouldHide>();
      }
    }

    entity.get<ui::HasDropdownState>().on = nv;
  });
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

    for (auto id : entity.get<HasChildrenComponent>().children) {
      auto opt_child = EQ().whereID(id).gen_first();
      if (nv) {
        opt_child->removeComponent<ShouldHide>();
      } else {
        opt_child->addComponent<ShouldHide>();
      }
    }

    entity.get<WRDS>().on = nv;

    if (!nv) {
      WRDS &wrds = entity.get<WRDS>();
      wrds.write_value_change_to_provider(wrds);
    }
  };

  auto &dropdown = EntityHelper::createEntity();
  dropdown.addComponent<ui::UIComponent>();
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

} // namespace ui

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight, "wm-afterhours");
  raylib::SetTargetFPS(200);

  // sophie
  {
    auto &entity = EntityHelper::createEntity();
    input::add_singleton_components<InputAction>(entity, get_mapping());
    window_manager::add_singleton_components(entity, 200);
    entity.addComponent<ui::UIContext>();
  }

  float y = 200;
  float o = 0;
  float s = 75;
  ui::make_button(vec2{200, y + (o++ * s)});
  ui::make_button(vec2{200, y + (o++ * s)});
  ui::make_checkbox(vec2{200, y + (o++ * s)});
  ui::make_slider(vec2{200, y + (o++ * s)});
  ui::make_dropdown(vec2{200, y + (o++ * s)}, []() {
    return std::vector<std::string>{{"default", "option1", "option2"}};
  });
  ui::make_dropdown<window_manager::ProvidesAvailableWindowResolutions>(
      vec2{400, y});

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

  systems.register_update_system(std::make_unique<ui::BeginUIContextManager>());
  {
    systems.register_update_system(std::make_unique<ui::HandleTabbing>());
    systems.register_update_system(std::make_unique<ui::HandleClicks>());
    systems.register_update_system(std::make_unique<ui::HandleDrags>());
    systems.register_update_system(
        std::make_unique<ui::UpdateDropdownOptions>());
  }
  systems.register_update_system(std::make_unique<ui::EndUIContextManager>());

  // renders
  {
    systems.register_render_system(
        [&]() { raylib::ClearBackground(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<ui::RenderUIComponents>());
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
