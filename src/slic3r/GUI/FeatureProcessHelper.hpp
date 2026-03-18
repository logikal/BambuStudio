#ifndef slic3r_GUI_FeatureProcessHelper_hpp_
#define slic3r_GUI_FeatureProcessHelper_hpp_

#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Slicing.hpp"
#include "libslic3r/libslic3r.h"

#include <cmath>
#include <string>
#include <vector>

namespace Slic3r::GUI {

enum class WallProcessCompatibilityIssue {
    None,
    MissingPreset,
    WrongPrinter,
    WrongNozzle,
    CoarserThanObject,
    NonDivisorCadence,
    BelowMinLayerHeight,
    AboveMaxLayerHeight
};

struct WallProcessCompatibility
{
    WallProcessCompatibilityIssue issue               { WallProcessCompatibilityIssue::None };
    std::string                   preset_name;
    double                        scope_layer_height  { 0.0 };
    double                        preset_layer_height { 0.0 };
    double                        derived_layer_height{ 0.0 };

    bool supported() const { return issue == WallProcessCompatibilityIssue::None; }
    bool changes_layer_height() const
    {
        return supported() && std::abs(derived_layer_height - scope_layer_height) > EPSILON;
    }
};

inline bool wall_layer_height_is_divisor(double coarse_height, double fine_height)
{
    if (fine_height <= EPSILON || coarse_height + EPSILON < fine_height)
        return false;

    const double ratio = coarse_height / fine_height;
    const double rounded_ratio = std::round(ratio);
    return rounded_ratio >= 1.0 && std::abs(ratio - rounded_ratio) <= 1e-3;
}

inline WallProcessCompatibility evaluate_wall_process_preset(
    const PresetBundle          &preset_bundle,
    const DynamicPrintConfig    &context_config,
    const std::string           &preset_name,
    size_t                       filament_slot,
    const std::vector<int>      &filament_maps)
{
    WallProcessCompatibility result;
    result.preset_name = preset_name;

    const Preset *preset = preset_bundle.prints.find_preset(preset_name, false);
    if (preset == nullptr) {
        result.issue = WallProcessCompatibilityIssue::MissingPreset;
        return result;
    }

    result.scope_layer_height = context_config.has("layer_height") ?
        context_config.option("layer_height")->getFloat() :
        0.0;
    result.preset_layer_height = preset->config.has("layer_height") ?
        preset->config.option("layer_height")->getFloat() :
        result.scope_layer_height;
    result.derived_layer_height = result.scope_layer_height;

    const std::string slot_nozzle_label = preset_bundle.get_filament_slot_nozzle_label(filament_slot, filament_maps, nullptr);
    const std::string preset_nozzle_label = preset_bundle.get_print_preset_nozzle_label(preset_name);
    if (!preset_nozzle_label.empty() && !slot_nozzle_label.empty() && preset_nozzle_label != slot_nozzle_label) {
        result.issue = WallProcessCompatibilityIssue::WrongNozzle;
        return result;
    }

    if (!preset_bundle.print_preset_matches_slot_nozzle(preset_name, filament_slot, filament_maps, nullptr)) {
        result.issue = WallProcessCompatibilityIssue::WrongPrinter;
        return result;
    }

    if (result.scope_layer_height <= EPSILON || result.preset_layer_height <= EPSILON)
        return result;

    if (result.preset_layer_height > result.scope_layer_height + EPSILON) {
        result.issue = WallProcessCompatibilityIssue::CoarserThanObject;
        return result;
    }

    if (std::abs(result.preset_layer_height - result.scope_layer_height) <= EPSILON)
        return result;

    if (!wall_layer_height_is_divisor(result.scope_layer_height, result.preset_layer_height)) {
        result.issue = WallProcessCompatibilityIssue::NonDivisorCadence;
        return result;
    }

    const int extruder_id = int(filament_slot) + 1;
    const double min_layer_height = Slicing::min_layer_height_from_nozzle(context_config, extruder_id);
    const double max_layer_height = Slicing::max_layer_height_from_nozzle(context_config, extruder_id);
    if (result.preset_layer_height + EPSILON < min_layer_height) {
        result.issue = WallProcessCompatibilityIssue::BelowMinLayerHeight;
        return result;
    }
    if (result.preset_layer_height > max_layer_height + EPSILON) {
        result.issue = WallProcessCompatibilityIssue::AboveMaxLayerHeight;
        return result;
    }

    result.derived_layer_height = result.preset_layer_height;
    return result;
}

} // namespace Slic3r::GUI

#endif
