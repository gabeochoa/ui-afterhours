#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <vector>

#include "ui_demo/input_mapping.h"

struct PlaybackStep {
  std::vector<InputAction> pressed;
  std::vector<InputAction> held;
};

struct PlaybackConfig {
  std::vector<PlaybackStep> steps;
  bool auto_quit = false;
  std::string dump_path = "ui_tree.json";
  // Per-run delay is configured via CLI, not TOML, to keep tests deterministic
  std::string scenario_name;
};

extern std::optional<PlaybackConfig> g_playback_config;
extern std::atomic<bool> g_should_quit;
