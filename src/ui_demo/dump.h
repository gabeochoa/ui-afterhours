#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "afterhours/src/plugins/ui.h"
#include "afterhours/src/plugins/ui/components.h"

inline void dump_ui_tree_json(const std::string &path) {
  using namespace afterhours::ui;
  afterhours::Entity &root_ent = afterhours::EntityQuery()
                                     .whereHasComponent<AutoLayoutRoot>()
                                     .gen_first_enforce();
  std::function<void(afterhours::EntityID, std::stringstream &, int)> rec;
  rec = [&](afterhours::EntityID id, std::stringstream &ss, int depth) {
    auto opt = afterhours::EntityHelper::getEntityForID(id);
    if (!opt) {
      ss << "{\"id\":" << id
         << ",\"name\":\"missing\",\"rect\":{\"x\":0,\"y\":0,\"w\":0,\"h\":0},"
            "\"children\":[]}";
      return;
    }
    afterhours::Entity &e = opt.asE();
    if (!e.has<UIComponent>()) {
      ss << "{\"id\":" << id
         << ",\"name\":\"no_uicmp\",\"rect\":{\"x\":0,\"y\":0,\"w\":0,\"h\":0},"
            "\"children\":[]}";
      return;
    }
    UIComponent &cmp = e.get<UIComponent>();
    RectangleType r = cmp.rect();
    std::string name = e.has<UIComponentDebug>()
                           ? e.get<UIComponentDebug>().name()
                           : std::string("unknown");
    ss << "{\"id\":" << id << ",\"name\":\"" << name << "\",";
    ss << "\"rect\":{\"x\":" << r.x << ",\"y\":" << r.y << ",\"w\":" << r.width
       << ",\"h\":" << r.height << "},";
    ss << "\"children\":[";
    for (size_t i = 0; i < cmp.children.size(); ++i) {
      rec(cmp.children[i], ss, depth + 1);
      if (i + 1 < cmp.children.size())
        ss << ",";
    }
    ss << "]}";
  };
  std::stringstream ss;
  ss << "{";
  ss << "\"root\":";
  rec(root_ent.id, ss, 0);
  ss << "}";
  std::ofstream out(path);
  if (out)
    out << ss.str();
}
