# Pressure Advance in Filament Profiles - Implementation Plan

## Overview

Enable pressure advance settings in filament profiles for all printers, providing sensible material-specific defaults while preserving Bambu's AMS-based pressure advance system as the primary mechanism for Bambu printers.

## Current State Analysis

### Existing Implementation
Pressure advance is **currently implemented but disabled for Bambu printers**:

1. **Configuration exists** (`src/libslic3r/PrintConfig.cpp:1744-1754`):
   - `enable_pressure_advance` (boolean, default: false)
   - `pressure_advance` (float, default: 0.02)

2. **GUI support exists** (`src/slic3r/GUI/Tab.cpp:3441-3442`):
   - Both settings appear in filament tab
   - **Hidden for Bambu printers** (`src/slic3r/GUI/Tab.cpp:3761-3767`)

3. **G-code generation implemented** (`src/libslic3r/GCodeWriter.cpp:254-270`):
   - Klipper: `SET_PRESSURE_ADVANCE ADVANCE=X`
   - RepRap: `M572 D0 SX`
   - Marlin: `M900 KX`
   - Bambu: Placeholder exists but disabled (`line 258`: `if (false) { // todo: bbl printer`)

### Current Restrictions
- **Excluded from Bambu printers** (`src/libslic3r/GCode.cpp:999, 6517`):
  ```cpp
  if (!gcodegen.is_BBL_Printer() && gcodegen.config().enable_pressure_advance.get_at(new_filament_id))
  ```
- **Not in variant system** - Missing from `filament_options_with_variant` (`src/libslic3r/PrintConfig.cpp:6160-6190`)

## Implementation Plan

### Step 1: Add to Variant System
**File**: `src/libslic3r/PrintConfig.cpp`  
**Lines**: 6160-6190

Add pressure advance options to the filament variant system:
```cpp
std::set<std::string> filament_options_with_variant = {
    // ... existing options ...
    "enable_pressure_advance",
    "pressure_advance",
    // ... rest of options ...
};
```

**Purpose**: Enables proper per-filament storage and multi-extruder support.

### Step 2: Update Base Filament Profiles
**File**: `resources/profiles/*/filament/fdm_filament_common.json`

Add default values to the base filament configuration:
```json
{
    "enable_pressure_advance": ["0"],
    "pressure_advance": ["0.0"]
}
```

**Purpose**: Provides system-wide defaults that can be overridden by specific materials.

### Step 3: Enable GUI for All Printers
**File**: `src/slic3r/GUI/Tab.cpp`  
**Lines**: 3761-3767

Remove Bambu printer restriction:
```cpp
// OLD: Hidden for Bambu printers
toggle_line("enable_pressure_advance", !is_BBL_printer);
if (is_BBL_printer)
    toggle_line("pressure_advance", false);

// NEW: Always show for all printers
toggle_line("enable_pressure_advance", true);
toggle_line("pressure_advance", true);
toggle_option("pressure_advance", m_config->opt_bool("enable_pressure_advance", 0));
```

**Purpose**: Makes pressure advance settings visible and configurable for all printer types.

### Step 4: Update G-code Generation Logic
**File**: `src/libslic3r/GCode.cpp`  
**Lines**: 999, 6517

Enable pressure advance for all printers with filament profile fallback:
```cpp
// OLD: Excluded Bambu printers
if (!gcodegen.is_BBL_Printer() && gcodegen.config().enable_pressure_advance.get_at(new_filament_id))

// NEW: Use filament profile as default for all printers
if (gcodegen.config().enable_pressure_advance.get_at(new_filament_id)) {
    // AMS values can still override this in Bambu's system
    gcode += gcodegen.writer().set_pressure_advance(gcodegen.config().pressure_advance.get_at(new_filament_id));
}
```

**Purpose**: Provides filament profile values as defaults while allowing AMS overrides.

### Step 5: Enable Bambu G-code Output
**File**: `src/libslic3r/GCodeWriter.cpp`  
**Lines**: 254-270

Implement Bambu-specific pressure advance G-code:
```cpp
std::string GCodeWriter::set_pressure_advance(double pa) const
{
    std::ostringstream gcode;
    if (pa < 0) return gcode.str();
    
    if (this->m_is_bbl_printer) {
        // Use Bambu-specific pressure advance G-code
        gcode << "M900 K" << std::setprecision(4) << pa << " L1000 M10 ; Set pressure advance from filament profile\n";
    } else {
        if (this->config.gcode_flavor == gcfKlipper)
            gcode << "SET_PRESSURE_ADVANCE ADVANCE=" << std::setprecision(4) << pa << "; Override pressure advance value\n";
        else if (this->config.gcode_flavor == gcfRepRapFirmware)
            gcode << ("M572 D0 S") << std::setprecision(4) << pa << "; Override pressure advance value\n";
        else
            gcode << "M900 K" << std::setprecision(4) << pa << "; Override pressure advance value\n";
    }
    return gcode.str();
}
```

**Purpose**: Generates appropriate G-code for Bambu printers using their specific format.

### Step 6: Add Material-Specific Values
**Files**: Individual filament profile JSON files

Add material-specific pressure advance values:

- **PLA profiles**: `"pressure_advance": ["0.02"]`
- **PETG profiles**: `"pressure_advance": ["0.08"]`
- **ABS profiles**: `"pressure_advance": ["0.06"]`
- **TPU profiles**: `"pressure_advance": ["0.1"]`
- **PC profiles**: `"pressure_advance": ["0.07"]`
- **ASA profiles**: `"pressure_advance": ["0.06"]`

**Example locations**:
- `resources/profiles/BBL/filament/fdm_filament_pla.json`
- `resources/profiles/BBL/filament/fdm_filament_pet.json`
- `resources/profiles/Anker/filament/fdm_filament_pla.json`
- etc.

## Architecture Integration

### Filament Variant System
The variant system (`filament_options_with_variant`) ensures:
- Proper per-extruder configuration in multi-material setups
- Automatic scaling based on `filament_extruder_variant`
- Integration with profile inheritance system

**Related code**: `src/libslic3r/PrintApply.cpp:1248`, `src/libslic3r/PresetBundle.cpp:117`

### Configuration Hierarchy
1. **Base default**: `fdm_filament_common.json` (0.0, disabled)
2. **Material default**: `fdm_filament_pla.json` (0.02, enabled for PLA)
3. **Specific profile**: `Bambu PLA Basic @BBL.json` (can override)
4. **User customization**: GUI modifications
5. **Runtime override**: AMS values (Bambu printers only)

## Benefits

### For All Users
- **Sensible defaults**: No manual pressure advance configuration needed
- **Material-aware**: Different values for PLA, PETG, ABS, etc.
- **Per-filament customization**: Fine-tune for specific brands/types

### For Bambu Printer Users
- **AMS integration preserved**: AMS values still take priority
- **Fallback system**: Profile values used when AMS data unavailable
- **Consistent experience**: Same interface as other printers

### For Non-Bambu Printer Users  
- **Full functionality**: Complete pressure advance control
- **Profile-based**: Values saved in filament presets
- **Multi-material support**: Per-extruder configuration

## Compatibility

### Backward Compatibility
- **Existing profiles**: Work unchanged (inherit common defaults)
- **Existing G-code**: No impact on current print files
- **AMS behavior**: Preserved for Bambu printers

### Multi-Printer Support
- **Mixed environments**: Same profiles work across printer types
- **Vendor-specific**: Each vendor directory can have optimal values
- **Printer-specific**: Machine profiles can override if needed

## Testing Strategy

### Unit Tests
1. **Configuration system**: Verify variant handling in `tests/libslic3r/`
2. **G-code generation**: Test pressure advance output for each printer type
3. **Profile inheritance**: Ensure values propagate correctly

### Integration Tests
1. **GUI functionality**: Verify controls appear and function correctly
2. **Profile loading**: Test filament switching with pressure advance
3. **Multi-material**: Verify per-extruder values in multi-color prints

### Validation Prints
1. **Calibration objects**: Use existing pressure advance test models
2. **Multi-material**: Test filament changes with different PA values
3. **Cross-platform**: Verify on Bambu and non-Bambu printers

## Implementation Order

1. **Step 1**: Add to variant system (enables infrastructure)
2. **Step 2**: Update base profiles (provides defaults)
3. **Step 3**: Enable GUI (allows user interaction)
4. **Step 4**: Update G-code logic (enables functionality)
5. **Step 5**: Implement Bambu G-code (completes support)
6. **Step 6**: Add material values (provides optimization)

## References

### Key Files Modified
- `src/libslic3r/PrintConfig.cpp` - Configuration definitions and variant system
- `src/libslic3r/GCode.cpp` - G-code generation logic
- `src/libslic3r/GCodeWriter.cpp` - Pressure advance G-code output
- `src/slic3r/GUI/Tab.cpp` - Filament tab GUI controls
- `resources/profiles/*/filament/*.json` - Filament profile configurations

### Related Systems
- **Calibration system**: `src/libslic3r/Calib.cpp` - Pressure advance calibration
- **Variant handling**: `src/libslic3r/ParameterUtils.cpp:58` - Multi-extruder support
- **Profile system**: `src/libslic3r/PresetBundle.cpp` - Configuration loading
- **AMS integration**: `src/slic3r/GUI/Widgets/AMSControl.cpp` - Bambu-specific handling

### Documentation
- **CLAUDE.md**: Project structure and development guidelines
- **PrintConfig architecture**: Configuration hierarchy and inheritance
- **Filament profiles**: JSON structure and variant system