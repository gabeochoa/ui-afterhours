

//
#include <iostream>

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

namespace util {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace util

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

struct CycleBetweenResolutions
    : System<window_manager::ProvidesCurrentResolution,
             window_manager::ProvidesAvailableWindowResolutions> {
  const float cycle_reset = 1.f;
  float timer;
  float timerReset;
  size_t res_index = 0;

  CycleBetweenResolutions() : timer(cycle_reset), timerReset(cycle_reset) {}
  virtual ~CycleBetweenResolutions() {}

  virtual bool should_run(float dt) override {
    if (timer < 0) {
      timer = timerReset;
      return true;
    }
    timer -= dt;
    return false;
  }

  virtual void for_each_with(
      Entity &, window_manager::ProvidesCurrentResolution &pCurrentResolution,
      window_manager::ProvidesAvailableWindowResolutions &pAvailableResolutions,
      float) override {
    res_index =
        (res_index + 1) % pAvailableResolutions.available_resolutions.size();
    const auto &resolution =
        pAvailableResolutions.available_resolutions[res_index];
    raylib::SetWindowSize(resolution.width, resolution.height);
    pCurrentResolution.current_resolution = resolution;

    std::cout << resolution.width << "x" << resolution.height << std::endl;
  }
};

int main(void) {
  const int screenWidth = 720;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight, "wm-afterhours");
  raylib::SetTargetFPS(200);

  // sophie
  {
    auto &entity = EntityHelper::createEntity();
    window_manager::add_singleton_components(
        entity, window_manager::Resolution{screenWidth, screenHeight}, 200,
        get_resolutions());
  }

  SystemManager systems;

  // debug systems
  { window_manager::enforce_singletons(systems); }

  systems.register_update_system(std::make_unique<CycleBetweenResolutions>());

  // renders
  {
    systems.register_render_system(
        [&]() { raylib::ClearBackground(raylib::DARKGRAY); });
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
