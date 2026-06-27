#include "MaterialLibrary.hpp"

#include <algorithm>

#include <QSettings>
#include <QString>

#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

QSettings settings() { return QSettings("Cadino", "Cadino"); }

void write_one(QSettings& s, const MaterialPreset& m) {
    s.setValue("color/r", m.color.r);
    s.setValue("color/g", m.color.g);
    s.setValue("color/b", m.color.b);
    s.setValue("roughness", m.roughness);
    s.setValue("metallic", m.metallic);
    s.setValue("pattern", m.pattern);
    s.setValue("texture_path", QString::fromStdString(m.texture_path));
}

}  // namespace

MaterialLibrary& MaterialLibrary::instance() {
    static MaterialLibrary inst;
    static bool loaded = false;
    if (!loaded) {
        inst.load();
        loaded = true;
    }
    return inst;
}

void MaterialLibrary::load() {
    presets_.clear();
    QSettings s = settings();
    s.beginGroup("materials");
    const auto names = s.childGroups();
    for (const auto& name : names) {
        s.beginGroup(name);
        MaterialPreset m;
        m.name = name.toStdString();
        m.color.r = s.value("color/r", m.color.r).toFloat();
        m.color.g = s.value("color/g", m.color.g).toFloat();
        m.color.b = s.value("color/b", m.color.b).toFloat();
        m.roughness = s.value("roughness", m.roughness).toFloat();
        m.metallic = s.value("metallic", m.metallic).toFloat();
        m.pattern = s.value("pattern", m.pattern).toInt();
        m.texture_path = s.value("texture_path").toString().toStdString();
        presets_.push_back(std::move(m));
        s.endGroup();
    }
    s.endGroup();
    std::sort(presets_.begin(), presets_.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });
}

void MaterialLibrary::save_to_disk() const {
    QSettings s = settings();
    s.beginGroup("materials");
    s.remove("");  // wipe and rewrite
    for (const auto& m : presets_) {
        s.beginGroup(QString::fromStdString(m.name));
        write_one(s, m);
        s.endGroup();
    }
    s.endGroup();
}

bool MaterialLibrary::capture_from_selection(const cadino::core::Document& doc,
                                             const std::vector<Selection>& sels,
                                             const std::string& name) {
    MaterialPreset m;
    m.name = name;
    bool captured = false;
    for (const auto& sel : sels) {
        if (sel.kind == SelectKind::Wall) {
            if (const auto* w = doc.find_wall(sel.id)) {
                m.color = w->color;
                m.roughness = w->roughness;
                m.metallic = w->metallic;
                m.pattern = w->pattern;
                m.texture_path = w->texture_path;
                captured = true;
                break;
            }
        } else if (sel.kind == SelectKind::Box) {
            if (const auto* b = doc.find_box(sel.id)) {
                m.color = b->color;
                m.roughness = b->roughness;
                m.metallic = b->metallic;
                m.pattern = b->pattern;
                m.texture_path = b->texture_path;
                captured = true;
                break;
            }
        } else if (sel.kind == SelectKind::Cylinder) {
            if (const auto* c = doc.find_cylinder(sel.id)) {
                m.color = c->color;
                m.roughness = c->roughness;
                m.metallic = c->metallic;
                m.pattern = c->pattern;
                m.texture_path = c->texture_path;
                captured = true;
                break;
            }
        }
    }
    if (!captured) return false;
    const auto it = std::find_if(presets_.begin(), presets_.end(),
        [&](const auto& p) { return p.name == name; });
    if (it != presets_.end()) *it = m;
    else presets_.push_back(m);
    std::sort(presets_.begin(), presets_.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });
    save_to_disk();
    return true;
}

int MaterialLibrary::apply_to_selection(cadino::core::Document& doc,
                                        cadino::core::CommandStack& stack,
                                        const std::vector<Selection>& sels,
                                        const std::string& name) const {
    const auto it = std::find_if(presets_.begin(), presets_.end(),
        [&](const auto& p) { return p.name == name; });
    if (it == presets_.end()) return 0;
    const auto& m = *it;

    int applied = 0;
    for (const auto& sel : sels) {
        if (sel.kind == SelectKind::Wall) {
            const auto* w = doc.find_wall(sel.id);
            if (!w) continue;
            cadino::core::Wall after = *w;
            after.color = m.color;
            after.roughness = m.roughness;
            after.metallic = m.metallic;
            after.pattern = m.pattern;
            after.texture_path = m.texture_path;
            stack.execute(std::make_unique<cadino::core::ModifyWallCommand>(sel.id, std::move(after)));
            ++applied;
        } else if (sel.kind == SelectKind::Box) {
            const auto* b = doc.find_box(sel.id);
            if (!b) continue;
            cadino::core::Box after = *b;
            after.color = m.color;
            after.roughness = m.roughness;
            after.metallic = m.metallic;
            after.pattern = m.pattern;
            after.texture_path = m.texture_path;
            stack.execute(std::make_unique<cadino::core::ModifyBoxCommand>(sel.id, std::move(after)));
            ++applied;
        } else if (sel.kind == SelectKind::Cylinder) {
            const auto* c = doc.find_cylinder(sel.id);
            if (!c) continue;
            cadino::core::Cylinder after = *c;
            after.color = m.color;
            after.roughness = m.roughness;
            after.metallic = m.metallic;
            after.pattern = m.pattern;
            after.texture_path = m.texture_path;
            stack.execute(std::make_unique<cadino::core::ModifyCylinderCommand>(sel.id, std::move(after)));
            ++applied;
        }
    }
    return applied;
}

void MaterialLibrary::remove(const std::string& name) {
    presets_.erase(
        std::remove_if(presets_.begin(), presets_.end(),
            [&](const auto& p) { return p.name == name; }),
        presets_.end());
    save_to_disk();
}

}  // namespace cadino::ui
