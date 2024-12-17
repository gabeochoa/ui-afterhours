

//
#include <iostream>
#include <set>

#include "rl.h"
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

std::vector<window_manager::Resolution> get_resolutions() {
  int count = 0;
  const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);
  std::vector<window_manager::Resolution> resolutions;

  for (int i = 0; i < count; i++) {
    resolutions.push_back(
        window_manager::Resolution{modes[i].width, modes[i].height});
  }
  // Sort the vector
  std::sort(resolutions.begin(), resolutions.end());

  // Remove duplicates
  resolutions.erase(std::unique(resolutions.begin(), resolutions.end()),
                    resolutions.end());

  return resolutions;
}

struct RenderAvailableResolutions
    : System<window_manager::ProvidesCurrentResolution,
             window_manager::ProvidesAvailableWindowResolutions> {
  virtual ~RenderAvailableResolutions() {}
  virtual void for_each_with(
      const Entity &,
      const window_manager::ProvidesCurrentResolution &pCurrentResolution,
      const window_manager::ProvidesAvailableWindowResolutions
          &pAvailableResolutions,
      float) const override {

    for (size_t i = 0; i < pAvailableResolutions.available_resolutions.size();
         i++) {
      const window_manager::Resolution &rez =
          pAvailableResolutions.available_resolutions[i];

      bool is_selected = rez == pCurrentResolution.current_resolution;

      DrawText(raylib::TextFormat("%i x %i %s", rez.width, rez.height,
                                  (is_selected ? " <-------- :) " : "")),
               30, 20 + (int)(15 * i), 10, raylib::RAYWHITE);
    }
  }
};

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
    return raylib::Rectangle{position.x - rw, position.y - rw,
                             size.x + (2 * rw), size.y + (2 * rw)};
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

struct HasLabel : BaseComponent {
  std::string label;
  HasLabel(const std::string &str) : label(str) {}
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

struct HandleTabbing : SystemWithUIContext<> {
  virtual ~HandleTabbing() {}

  virtual void for_each_with(Entity &entity, UIComponent &, float) override {
    if (!context_entity)
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
      DrawText(entity.get<HasLabel>().label.c_str(), transform.position.x,
               transform.position.y, transform.size.y, raylib::RAYWHITE);
    }
  }
};

void make_button(vec2 position) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<ui::UIComponent>();
  entity.addComponent<ui::Transform>(position, vec2{50, 25});
  entity.addComponent<ui::HasColor>(raylib::BLUE);
  entity.addComponent<ui::HasLabel>(raylib::TextFormat("button%i", entity.id));
  entity.addComponent<ui::HasClickListener>([](Entity &entity) {
    std::cout << "I clicked the button " << entity.id << std::endl;
  });
}

} // namespace ui

int main(void) {
  const int screenWidth = 720;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight, "wm-afterhours");
  raylib::SetTargetFPS(200);

  // sophie
  {
    auto &entity = EntityHelper::createEntity();
    input::add_singleton_components<InputAction>(entity, get_mapping());
    window_manager::add_singleton_components(
        entity, window_manager::Resolution{screenWidth, screenHeight}, 200,
        get_resolutions());
    entity.addComponent<ui::UIContext>();
  }

  ui::make_button(vec2{200, 200});
  ui::make_button(vec2{200, 250});
  ui::make_button(vec2{200, 300});

  SystemManager systems;

  // debug systems
  {
    window_manager::enforce_singletons(systems);
    input::enforce_singletons<InputAction>(systems);
  }

  // external plugins
  { input::register_update_systems<InputAction>(systems); }

  systems.register_update_system(std::make_unique<ui::BeginUIContextManager>());
  systems.register_update_system(std::make_unique<ui::HandleTabbing>());
  systems.register_update_system(std::make_unique<ui::HandleClicks>());
  systems.register_update_system(std::make_unique<ui::EndUIContextManager>());

  // renders
  {
    systems.register_render_system(
        [&]() { raylib::ClearBackground(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<ui::RenderUIComponents>());
    systems.register_render_system(
        std::make_unique<RenderAvailableResolutions>());
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
