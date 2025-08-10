#pragma once

#include <fstream>
#include <string>

#include "afterhours/src/plugins/ui.h"
#include "afterhours/src/plugins/ui/components.h"
#include <nlohmann/json.hpp>

inline void dump_ui_tree_json(const std::string &path) {
  using namespace afterhours::ui;
  afterhours::Entity &root_ent = afterhours::EntityQuery()
                                     .whereHasComponent<AutoLayoutRoot>()
                                     .gen_first_enforce();

  std::function<nlohmann::json(afterhours::EntityID)> rec_json;
  rec_json = [&](afterhours::EntityID id) -> nlohmann::json {
    nlohmann::json node;
    node["id"] = id;

    auto opt = afterhours::EntityHelper::getEntityForID(id);
    if (!opt) {
      node["name"] = "missing";
      node["rect"] = { {"x", 0}, {"y", 0}, {"w", 0}, {"h", 0} };
      node["children"] = nlohmann::json::array();
      return node;
    }

    afterhours::Entity &e = opt.asE();
    if (!e.has<UIComponent>()) {
      node["name"] = "no_uicmp";
      node["rect"] = { {"x", 0}, {"y", 0}, {"w", 0}, {"h", 0} };
      node["children"] = nlohmann::json::array();
      return node;
    }

    UIComponent &cmp = e.get<UIComponent>();
    RectangleType r = cmp.rect();
    const std::string name = e.has<UIComponentDebug>()
                                 ? e.get<UIComponentDebug>().name()
                                 : std::string("unknown");

    node["name"] = name;
    node["rect"] = { {"x", r.x}, {"y", r.y}, {"w", r.width}, {"h", r.height} };

    nlohmann::json children = nlohmann::json::array();
    for (size_t i = 0; i < cmp.children.size(); ++i) {
      children.push_back(rec_json(cmp.children[i]));
    }
    node["children"] = std::move(children);
    return node;
  };

  nlohmann::json root;
  root["root"] = rec_json(root_ent.id);

  std::ofstream out(path);
  if (out) {
    out << root.dump(2);
  }
}
