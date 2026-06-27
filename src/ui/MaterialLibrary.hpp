#pragma once

#include <string>
#include <vector>

#include "Selection.hpp"
#include "entity/Color.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

struct MaterialPreset {
    std::string name;
    cadino::core::Color color{0.78f, 0.78f, 0.80f};
    float roughness{0.6f};
    float metallic{0.0f};
    int pattern{0};
    std::string texture_path;
};

// QSettings-backed material preset registry. Survives across sessions
// at the org/app scope ("Cadino"/"Cadino").
class MaterialLibrary {
public:
    static MaterialLibrary& instance();

    [[nodiscard]] const std::vector<MaterialPreset>& presets() const noexcept { return presets_; }

    void load();
    void save_to_disk() const;

    // Capture the material parameters of the first material-bearing entity
    // in `sels` (wall/box/cylinder), store under `name`, persist. Returns
    // true if a preset was created or updated.
    bool capture_from_selection(const cadino::core::Document& doc,
                                const std::vector<Selection>& sels,
                                const std::string& name);

    // Apply the named preset to every material-bearing entity in `sels`
    // via the appropriate Modify command. Returns the number applied.
    int apply_to_selection(cadino::core::Document& doc,
                           cadino::core::CommandStack& stack,
                           const std::vector<Selection>& sels,
                           const std::string& name) const;

    void remove(const std::string& name);

private:
    MaterialLibrary() = default;
    std::vector<MaterialPreset> presets_;
};

}  // namespace cadino::ui
