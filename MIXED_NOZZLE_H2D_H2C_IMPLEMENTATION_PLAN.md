# Mixed-Nozzle H2D/H2C Implementation Plan

## Goal

Implement first-pass support for mixed nozzle diameters on:

- `H2D` / `H2D Pro`: heterogeneous per-extruder nozzle diameters.
- `H2C`: existing nozzle-rack / nozzle-group sync model, with one synchronized nozzle option per print.

The intended slicing behavior is:

- separate objects may use independent Z grids
- same-object mixed-nozzle support is limited to validated compatible-cadence cases
- no nozzle may be forced to print a layer height it cannot support
- the default wipe/prime architecture remains one global tower driven by merged print-wide toolchange events

## Non-Goals For This Pass

- full arbitrary same-object independent Z schedules
- per-extruder wipe/prime towers
- simultaneous mixed H2C nozzle-group slicing in one print
- cross-object supports across independent object grids

## Key Product Rules

### H2D / H2D Pro

- `nozzle_diameter` is authoritative per logical extruder.
- Mixed presets such as `0.2/0.4`, `0.4/0.6`, `0.4/0.8` must be representable and preserved.
- A coarse nozzle may only print on layers that match its allowed cadence and layer-height limits.

### H2C

- Keep the existing rack/group abstraction.
- A print uses one synchronized nozzle option, selected from the device/preset nozzle-group state.
- Do not reinterpret H2C as a true simultaneous seven-nozzle slicer in this pass.

### Layer-Grid Rules

- Separate objects:
  - allowed to have independent Z grids
  - merged into one print-wide event timeline by `print_z`
- Same object:
  - only allow compatible-cadence cases
  - reject any geometry assignment that would require a coarse nozzle to participate on fine-only layers

Examples:

- `0.2 @ 0.2` with `0.4 @ 0.4` in one object: potentially allowed
- `0.2 @ 0.08` with `0.4` only on legal coarse cadence layers: potentially allowed
- `0.2 @ 0.08` with `0.4` needed on every `0.08` layer: reject
- `0.3` with `0.4` in the same object: reject
- `0.3` object next to `0.4` object: allow

### Wipe/Prime Tower Rules

- Keep one global tower by default.
- Tower planning must be driven by the merged toolchange event stream from both extruders and AMS color changes.
- Tower layers exist where flushing/priming is actually needed, not just where a single shared object layer schedule exists.
- If a mixed-grid combination cannot be made safe with one global tower, block that combination first. Do not add per-extruder towers in v1.

## Implementation Strategy

### Phase 1: Validation And Capability Groundwork

Add explicit validation and classification before broad behavior changes:

- classify prints into:
  - shared-grid
  - separate-object independent-grid
  - same-object compatible-cadence
  - unsupported
- distinguish:
  - heterogeneous nozzle vector support
  - independent object-grid support
  - same-object compatible-cadence support
  - H2C nozzle-option-sync support
- reject unsupported combinations with targeted messages instead of silently falling back to nozzle `0`

Recommended first code changes:

- add grid/capability helpers in `Slicing.hpp` / `Slicing.cpp`
- add print-level validation in `Print::validate()`
- remove hard validation that prime tower requires identical nozzle diameters
- preserve existing hard rejections where the pipeline truly cannot handle a case yet

### Phase 2: H2D Preset And Matching Support

- ensure `H2D` / `H2D Pro` printer presets can preserve different per-extruder `nozzle_diameter` values
- update machine/preset matching to compare the full per-extruder nozzle vector, not just extruder `0`
- update sync / mismatch UX to show per-extruder mismatches

### Phase 3: Separate-Object Independent Z Grids

Build on the existing fact that print-wide layer collection already merges each object's `print_z` values:

- keep per-object slicing schedules independent
- keep print-wide ordering based on merged `print_z` events
- verify tool ordering and tower planning tolerate non-identical object schedules
- if tower planning still assumes equal object layer heights, convert it to event-based scheduling

### Phase 4: Same-Object Compatible-Cadence Support

- compute a base cadence/grid for participating tools
- validate that each tool only emits on its legal cadence layers
- reject same-object layouts that require cross-tool participation on incompatible layers
- keep this validation narrow and explicit; do not claim full arbitrary same-object independent Z support

### Phase 5: H2C Integration

- keep `H2C` on the nozzle-group / rack workflow
- make the selected nozzle option authoritative for slicing validation and print gating
- validate the selected option against the print's layer-grid mode
- do not expand H2C to simultaneous multi-group slicing in this phase

### Phase 6: Tower / Flush / AMS Integration

- keep one global tower
- feed it from the merged stream of:
  - extruder changes
  - same-extruder AMS color/material changes
  - nozzle-change / nozzle-group events where applicable
- ensure sparse / uneven event Zs remain valid
- if a tower case remains unsafe, reject that print combination with a targeted error

## Important Code Regions

These are the most relevant files and starting points for implementation.

### Core Layering And Slicing Parameters

- `src/libslic3r/Slicing.hpp:27-115`
  - `SlicingParameters`
  - `equal_layering()`
  - current model assumes one object-layer schedule can be compared structurally
- `src/libslic3r/Slicing.cpp:62-124`
  - `SlicingParameters::create_from_config()`
  - computes `min_layer_height` / `max_layer_height` from the set of object extruders
  - current result is still one `layer_height` plus min/max bounds, not a mixed-grid planner
- `src/libslic3r/PrintObject.cpp:3176-3211`
  - `PrintObject::slicing_parameters(...)`
  - collects the extruders used by an object, then calls `SlicingParameters::create_from_config()`
  - this is the right place to detect same-object multi-extruder geometry and feed richer grid validation

### Print Validation

- `src/libslic3r/Print.cpp:1296-1401`
  - current wipe-tower validation
  - hard-rejects different nozzle diameters when prime tower is enabled
  - also currently requires all objects to have the same layer heights for prime tower
- `src/libslic3r/Print.cpp:1404-1515`
  - layer-height / line-width validation
  - currently uses `min_nozzle_diameter` / `max_nozzle_diameter` across used extruders
  - important for adding heterogeneous-nozzle-safe validation without over-restricting separate-object cases
- `src/libslic3r/Print.cpp:1095-1167`
  - `Print::check_multi_filament_valid()`
  - existing multi-filament safety checks; useful anchor for adding new mixed-grid / mixed-nozzle validation messaging

### Print-Wide Layer Collection

- `src/libslic3r/GCode.cpp:1374-1428`
  - `GCode::collect_layers_to_print(const Print&)`
  - already merges each object's `print_z` events into a print-wide ordered sequence
  - this is the main reason separate-object independent Z grids are feasible
- `src/libslic3r/GCode.cpp:1257-1315`
  - `GCode::collect_layers_to_print(const PrintObject&)`
  - pairs object/support layers by `print_z` inside a single object
  - important boundary for same-object mixed-grid work

### Tool Ordering

- `src/libslic3r/GCode/ToolOrdering.cpp:400-460`
  - constructors for by-object and by-layer modes
  - currently initialize layers by merging all object/support `print_z` values
- `src/libslic3r/GCode/ToolOrdering.cpp:648-660`
  - `initialize_layers()`
  - merges nearly-equal Z values into `m_layer_tools`
- `src/libslic3r/GCode/ToolOrdering.cpp:815-909`
  - `fill_wipe_tower_partitions()`
  - tower partitioning and extra wipe-tower layer insertion currently depend on `max_layer_height` and global layer sequencing
- `src/libslic3r/GCode/ToolOrdering.cpp:1503-1528`
  - still synthesizes a one-extruder comparison result using `nozzle_diameter.get_at(0)`
  - direct example of a historical nozzle-`0` assumption that needs cleanup

### Wipe / Prime Tower

- `src/libslic3r/GCode/WipeTower.cpp:2835-2843`
  - `plan_toolchange()`
  - global tower plan keyed by `z`, `old_tool`, `new_tool`
  - already distinguishes same-extruder vs cross-extruder wipe volume selection
- `src/libslic3r/GCode/WipeTower.cpp:4417-4428`
  - `plan_tower_new()`
  - tower geometry/depth planning
- `src/libslic3r/GCode/WipeTower.cpp:4585-4603`
  - layer-by-layer tower generation loop over global `m_plan`
- `src/libslic3r/GCode/WipeTower.cpp:4772-4800`
  - `generate()`
  - global-tower generation entry point
- `src/libslic3r/GCode.cpp:1179-1204`
  - `WipeTowerIntegration` logic for sparse tower layers and `wipe_tower_z`
  - important for uneven / sparse merged event schedules

### H2D / H2C Preset Modeling

- `resources/profiles/BBL/machine/Bambu Lab H2D 0.4 nozzle.json:1-80`
  - H2D base machine preset
  - note that `nozzle_diameter` is already a 2-entry vector
- `resources/profiles/BBL/machine/Bambu Lab H2D 0.4 nozzle.json:271-304`
  - `printer_extruder_id` and `printer_extruder_variant`
  - useful when preserving variant-aware mixed-nozzle semantics
- `resources/profiles/BBL/machine/Bambu Lab H2C 0.4 nozzle.json:1-40`
  - H2C base machine preset
  - note `extruder_max_nozzle_count`
- `resources/profiles/BBL/machine/Bambu Lab H2C 0.4 nozzle.json:105-146`
  - `master_extruder_id`
  - useful for keeping H2C in its existing grouped nozzle model

### H2C / Multi-Nozzle Sync Model

- `src/libslic3r/MultiNozzleUtils.hpp:13-60`
  - `NozzleInfo`
  - `NozzleGroupInfo`
  - core data model for grouped nozzle options
- `src/slic3r/GUI/DeviceCore/DevNozzleSystem.cpp:512-551`
  - `DevNozzleSystem::GetExtruderNozzleInfo()`
  - machine-side view of installed nozzles, including rack nozzles on the main extruder
- `src/slic3r/GUI/Widgets/MultiNozzleSync.cpp:833-968`
  - `MultiNozzleSyncDialog`
  - existing user flow for selecting a nozzle option for a print
  - this should remain authoritative for H2C in v1

### Printer Matching And Sync UI

- `src/slic3r/GUI/Plater.cpp:17192-17215`
  - `get_printer_preset(const MachineObject*)`
  - currently matches presets by model plus `GetNozzleDiameter(0)` only
- `src/slic3r/GUI/SelectMachine.cpp:1848-1853`
  - `is_nozzle_hrc_matched()`
  - hardness/nozzle checks for print sending flow
- `src/slic3r/GUI/SelectMachine.cpp:4852-4874`
  - nozzle diameter mismatch message during print submission
  - already supports per-extruder wording for dual extruders
- `src/slic3r/GUI/SyncAmsInfoDialog.cpp:1883-1932`
  - `is_same_nozzle_diameters()`
  - currently compares used extruders to `GetNozzleDiameter(0)` and needs a full-vector fix
- `src/slic3r/GUI/SyncAmsInfoDialog.cpp:2355-2366`
  - nozzle-type/nozzle-count validation path during AMS sync flow

### Filament / Nozzle Compatibility

- `src/slic3r/GUI/PartPlate.cpp:1655-1732`
  - `check_compatible_of_nozzle_and_filament()`
  - currently uses `config.nozzle_diameter->values[0]`
  - must become per-used-extruder aware
- `src/slic3r/GUI/PartPlate.cpp:1734-1748`
  - `check_flow_compatible_of_nozzle_and_filament()`
  - already enters multi-nozzle flow compatibility logic; good place to align with mixed-nozzle support

## Concrete Tasks

### Task 1: Add Grid-Mode Classification

Introduce helper logic that classifies a print as:

- shared-grid
- separate-object independent-grid
- same-object compatible-cadence
- unsupported

This logic should use:

- the extruders used per object
- each object's `layer_height`
- nozzle min/max height limits derived from `SlicingParameters`
- whether the object is multi-material / uses multiple tool assignments

### Task 2: Relax Only The Safe Validation

In `Print::validate()`:

- remove the blanket "different nozzle diameters are not allowed with prime tower" rule
- replace it with:
  - full validation of mixed-nozzle capability
  - explicit rejection only for unsupported grid/tower combinations
- keep filament-diameter mismatch validation as-is unless there is a strong reason to change it

### Task 3: Preserve Separate-Object Independent Grids

Do not force all objects to have identical layer schedules when:

- the print can already be represented by merged `print_z` events
- tower/tool ordering logic can tolerate it

If a tower path still requires equal layer schedules, gate only that path rather than all heterogeneous-nozzle prints.

### Task 4: Same-Object Mixed-Cadence Validation

Implement only a narrow first-pass validator:

- allow same-object mixed-nozzle cases only if the object-layer cadence is compatible with all participating tools
- reject same-object layouts that would force a coarse nozzle onto fine-only layers
- do not try to implement arbitrary same-object independent Z schedules

### Task 5: Fix Nozzle-`0` Assumptions In User-Facing Flows

Update:

- machine preset matching in `Plater.cpp`
- sync validation in `SyncAmsInfoDialog.cpp`
- filament/nozzle compatibility in `PartPlate.cpp`
- any tool-ordering / stats path still synthesizing a fake single nozzle from index `0`

### Task 6: Keep H2C On The Existing Sync Model

- use `MultiNozzleSyncDialog` and `NozzleGroupInfo` as the H2C source of truth
- validate H2C's selected nozzle option against the print's grid mode
- do not re-architect H2C into simultaneous mixed-group slicing

## Suggested Test Matrix

### Validation

- allow separate-object `0.3` object + `0.4` object
- allow same-object `0.2 @ 0.2` + `0.4 @ 0.4` compatible-cadence case
- reject same-object `0.3` + `0.4`
- reject same-object `0.2 @ 0.08` when `0.4` would need to print every `0.08`

### Tower / Toolchange

- single global tower with AMS color changes on both extruders
- sparse / uneven merged event Zs still produce valid tower layers
- unsupported tower combinations emit targeted validation errors

### Matching / Sync

- H2D preset-machine matching must use the full nozzle vector
- sync flow must compare each used extruder against the corresponding machine nozzle

### H2C

- selected nozzle option from `MultiNozzleSyncDialog` remains authoritative
- unsupported simultaneous mixed-group use is rejected explicitly

## Recommended Shipping Shape

If the full implementation must be split:

1. Ship heterogeneous nozzle-vector support, per-extruder matching, and nozzle-`0` cleanup first.
2. Ship separate-object independent grids next, because the merged `print_z` path already exists.
3. Ship narrow same-object compatible-cadence support after that.
4. Keep H2C on the sync-based grouped-nozzle path throughout.

## Notes For The Next Agent

- The current code already contains some of the right abstractions, especially:
  - merged print-wide `print_z` collection
  - per-object extruder collection
  - H2C nozzle groups and sync UI
  - global tower planning by toolchange event
- The biggest traps are:
  - blanket same-layer-height assumptions in `Print::validate()`
  - nozzle-`0` compatibility / matching shortcuts
  - assuming same-object mixed-nozzle support can be solved by only relaxing validation

The safe approach is to loosen the rules only where the pipeline already has the right shape, and fail closed everywhere else.
