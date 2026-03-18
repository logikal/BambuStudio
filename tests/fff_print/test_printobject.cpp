#include <catch2/catch.hpp>

#include "libslic3r/GCodeReader.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/libslic3r.h"
#include "libslic3r/Print.hpp"
#include "libslic3r/Layer.hpp"

#include "test_data.hpp"

#include <cmath>
#include <cstdlib>
#include <set>

using namespace Slic3r;
using namespace Slic3r::Test;

namespace {

bool starts_with(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

size_t count_extruding_layers_for_tool(const std::string &gcode, int tool_id)
{
    GCodeReader reader;
    int current_tool = 0;
    std::set<int> layer_zs;
    reader.parse_buffer(gcode, [&current_tool, tool_id, &layer_zs](GCodeReader &self, const GCodeReader::GCodeLine &line) {
        const std::string_view cmd = line.cmd();
        if (cmd.size() >= 2 && cmd.front() == 'T')
            current_tool = std::atoi(std::string(cmd.substr(1)).c_str());
        else if (current_tool == tool_id && line.extruding(self) && line.dist_XY(self) > 0.f)
            layer_zs.insert(int(std::lround(self.z() * 1000.f)));
    });
    return layer_zs.size();
}

std::set<int> extrusion_heights_for_tool_and_role(const std::string &gcode, int tool_id, ExtrusionRole role)
{
    GCodeReader reader;
    int current_tool = 0;
    int current_role = int(erNone);
    double current_height = 0.;
    const std::string height_tag = GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Height);
    constexpr std::string_view role_tag = "_EXTRUSION_ROLE:";
    std::set<int> heights;

    reader.parse_buffer(gcode, [&current_tool, &current_role, &current_height, tool_id, role, &heights, &height_tag, role_tag](GCodeReader &self, const GCodeReader::GCodeLine &line) {
        const std::string_view cmd = line.cmd();
        if (cmd.size() >= 2 && cmd.front() == 'T') {
            current_tool = std::atoi(std::string(cmd.substr(1)).c_str());
            return;
        }

        const std::string_view comment = line.comment();
        if (! comment.empty()) {
            if (starts_with(comment, role_tag))
                current_role = std::atoi(std::string(comment.substr(role_tag.size())).c_str());
            else if (starts_with(comment, height_tag))
                current_height = std::atof(std::string(comment.substr(height_tag.size())).c_str());
        }

        if (current_tool == tool_id &&
            current_role == int(role) &&
            current_height > 0. &&
            line.extruding(self) &&
            line.dist_XY(self) > 0.f)
            heights.insert(int(std::lround(current_height * 1000.)));
    });

    return heights;
}

size_t expected_combined_infill_layers(const PrintObject &object, double target_height)
{
    size_t layers_with_infill = 0;
    double current_height = 0.;
    size_t num_layers = 0;
    const auto layers = object.layers();
    for (size_t layer_idx = 0; layer_idx < layers.size(); ++layer_idx) {
        const Layer *layer = layers[layer_idx];
        if (layer->id() == 0) {
            ++layers_with_infill;
            continue;
        }

        const double next_height = current_height + layer->height;
        if (next_height > target_height + EPSILON) {
            ++layers_with_infill;
            current_height = 0.;
            num_layers = 0;
        }

        current_height += layer->height;
        ++num_layers;
    }

    if (num_layers > 0)
        ++layers_with_infill;

    return layers_with_infill;
}

}

SCENARIO("PrintObject: object layer heights", "[PrintObject]") {
    GIVEN("20mm cube and default initial config, initial layer height of 2mm") {
        WHEN("generate_object_layers() is called for 2mm layer heights and nozzle diameter of 3mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		2 },
	            { "nozzle_diameter", 	3 }
	        });
            ConstLayerPtrsAdaptor layers = print.objects().front()->layers();
            THEN("The output vector has 10 entries") {
                REQUIRE(layers.size() == 10);
            }
            AND_THEN("Each layer is approximately 2mm above the previous Z") {
                coordf_t last = 0.0;
                for (size_t i = 0; i < layers.size(); ++ i) {
                    REQUIRE((layers[i]->print_z - last) == Approx(2.0));
                    last = layers[i]->print_z;
                }
            }
        }
        WHEN("generate_object_layers() is called for 10mm layer heights and nozzle diameter of 11mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		10 },
	            { "nozzle_diameter", 	11 }
	        });
            ConstLayerPtrsAdaptor layers = print.objects().front()->layers();
			THEN("The output vector has 3 entries") {
                REQUIRE(layers.size() == 3);
            }
            AND_THEN("Layer 0 is at 2mm") {
                REQUIRE(layers.front()->print_z == Approx(2.0));
            }
            AND_THEN("Layer 1 is at 12mm") {
                REQUIRE(layers[1]->print_z == Approx(12.0));
            }
        }
        WHEN("generate_object_layers() is called for 15mm layer heights and nozzle diameter of 16mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		15 },
	            { "nozzle_diameter", 	16 }
	        });
            ConstLayerPtrsAdaptor layers = print.objects().front()->layers();
			THEN("The output vector has 2 entries") {
                REQUIRE(layers.size() == 2);
            }
            AND_THEN("Layer 0 is at 2mm") {
                REQUIRE(layers[0]->print_z == Approx(2.0));
            }
            AND_THEN("Layer 1 is at 17mm") {
                REQUIRE(layers[1]->print_z == Approx(17.0));
            }
        }
#if 0
        WHEN("generate_object_layers() is called for 15mm layer heights and nozzle diameter of 5mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		15 },
	            { "nozzle_diameter", 	5 }
	        });
			const std::vector<Slic3r::Layer*> &layers = print.objects().front()->layers();
			THEN("The layer height is limited to 5mm.") {
                CHECK(layers.size() == 5);
                coordf_t last = 2.0;
                for (size_t i = 1; i < layers.size(); i++) {
                    REQUIRE((layers[i]->print_z - last) == Approx(5.0));
                    last = layers[i]->print_z;
                }
            }
        }
#endif
    }
}

SCENARIO("PrintObject: feature-process base layer height preserves coarse sparse infill cadence", "[PrintObject][FeatureProcess]") {
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({
        { "first_layer_height",                "0.1" },
        { "layer_height",                      "0.1" },
        { "feature_process_base_layer_height", "0.2" },
        { "nozzle_diameter",                   "0.2,0.4" },
        { "wall_filament",                     "1" },
        { "sparse_infill_filament",            "2" },
        { "solid_infill_filament",             "2" },
        { "bottom_shell_layers",               "0" },
        { "top_shell_layers",                  "0" },
        { "sparse_infill_density",             "15%" },
        { "infill_combination",                "0" },
        { "enable_support",                    "0" },
        { "enable_prime_tower",                "0" }
    });

    TriangleMesh cube = Slic3r::make_cube(20, 20, 1);
    Slic3r::Print print;
    Slic3r::Test::init_and_process_print({ cube }, print, config);

    REQUIRE(print.objects().size() == 1);
    REQUIRE(print.objects().front()->layers().size() == 10);

    const size_t expected_layers = expected_combined_infill_layers(*print.objects().front(), 0.2);
    const std::string gcode = Slic3r::Test::gcode(print);

    REQUIRE(count_extruding_layers_for_tool(gcode, 1) == expected_layers);
    REQUIRE(expected_layers < print.objects().front()->layers().size());
}

SCENARIO("PrintObject: feature-process base layer height preserves coarse solid-surface heights", "[PrintObject][FeatureProcess]") {
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({
        { "first_layer_height",                "0.2" },
        { "layer_height",                      "0.1" },
        { "feature_process_base_layer_height", "0.2" },
        { "nozzle_diameter",                   "0.2,0.4" },
        { "wall_filament",                     "1" },
        { "sparse_infill_filament",            "2" },
        { "solid_infill_filament",             "2" },
        { "bottom_shell_layers",               "2" },
        { "top_shell_layers",                  "2" },
        { "sparse_infill_density",             "15%" },
        { "infill_combination",                "0" },
        { "enable_support",                    "0" },
        { "enable_prime_tower",                "0" }
    });

    TriangleMesh cube = Slic3r::make_cube(20, 20, 4);
    Slic3r::Print print;
    Slic3r::Test::init_and_process_print({ cube }, print, config);

    const std::string gcode = Slic3r::Test::gcode(print);

    REQUIRE(extrusion_heights_for_tool_and_role(gcode, 1, erTopSolidInfill) == std::set<int>{200});
    REQUIRE(extrusion_heights_for_tool_and_role(gcode, 1, erBottomSurface) == std::set<int>{200});

    const std::set<int> solid_heights = extrusion_heights_for_tool_and_role(gcode, 1, erSolidInfill);
    REQUIRE(solid_heights.find(200) != solid_heights.end());
}
