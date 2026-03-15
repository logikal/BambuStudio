---
name: Fix configuration update dialog word wrapping
overview: Fix the word wrapping issue in the "Configuration update" dialog by setting proper width constraints on the heading text control and using a specific width value for the Wrap() call instead of -1.
todos: []
isProject: false
---

# Fix Configuration Update Dialog Word Wrapping

## Problem

The changelog content displayed in the configuration update dialog doesn't properly word wrap. The `changelog_textctrl` `wxStaticText` control (line 190) displays the update changelog text but doesn't have wrapping enabled, causing long lines to extend beyond the visible area.

## Solution

Add word wrapping to the `changelog_textctrl` control in `[src/slic3r/GUI/UpdateDialogs.cpp](src/slic3r/GUI/UpdateDialogs.cpp)` by:

1. Setting explicit width constraints (MinSize/MaxSize) to match the scroll window width
2. Calling `Wrap()` with a specific width value after the label is set

## Implementation Details

### File to Modify

- `[src/slic3r/GUI/UpdateDialogs.cpp](src/slic3r/GUI/UpdateDialogs.cpp)` - Lines 190, 219, 228

### Changes

1. **Set width constraints on changelog_textctrl**: After creating the control on line 190, add `SetMaxSize()` and `SetMinSize()` calls to constrain the width. The scroll window uses `FromDIP(560)` width, so use `FromDIP(560)` or slightly less (e.g., `FromDIP(545)`) to account for padding, similar to the `update_comment` control on lines 206-207.
2. **Add Wrap() call**: After the label is set (line 219) or before adding to the sizer (line 228), call `Wrap(FromDIP(560))` (or the same value used for MaxSize) to enable word wrapping. Since the label is built incrementally in a loop (line 219), the `Wrap()` call should be done after all updates have been processed, likely just before adding to the sizer on line 228.

### Reference Implementation

The code already has a working example of proper wrapping on lines 204-208 for the `update_comment` Label control:

- Sets `SetMaxSize(wxSize(FromDIP(545), -1))` and `SetMinSize(wxSize(FromDIP(545), -1))`
- Calls `Wrap(FromDIP(450))`

### How to Trigger the Configuration Update Dialog for Testing

To test the word wrapping fix, you need to make the application think there's a newer configuration package available. Here are the steps:

**1. Locate the Data Directory:**

- **macOS**: `~/Library/Application Support/BambuStudio` (or `BambuStudioInternal`/`BambuStudioBeta` if using internal/beta builds)
- **Linux**: `~/.config/BambuStudio`
- **Windows**: `%APPDATA%\BambuStudio`

**2. Find the Configuration Files:**

The application checks for updates by comparing versions in:

- **Installed config**: `{data_dir}/BBL/BBL.json` (contains current version)
- **Cache folder**: `{data_dir}/ota/slicer/settings/` (contains downloaded updates)

**3. Method 1: Lower the Installed Version (Easiest)**

a. Navigate to `{data_dir}/BBL/`

b. Open `BBL.json` in a text editor

c. Find the `"version"` field (it should look like `"version": "2.4.0.7"` or similar)

d. Change it to an older version (e.g., `"version": "2.3.0.0"`)

e. Save the file

f. Restart BambuStudio - the update check runs on startup and should detect the "newer" version

**4. Method 2: Place a Newer Version in Cache**

a. Navigate to `{data_dir}/ota/slicer/settings/`

b. If the folder doesn't exist, create it: `mkdir -p {data_dir}/ota/slicer/settings/`

c. Create or modify `BBL.changelog` file with:

```json
      {
        "version": "2.5.0.0",
        "force_upgrade": false,
        "description": "Test update"
      }
      

```

d. Create a `BBL.json` file in the cache folder with a newer version

e. Create the preset directories: `BBL/print`, `BBL/filament`, `BBL/machine`

f. Restart BambuStudio

**5. Alternative: Use the Menu Option (if available)**

Some builds may have a "Check for Configuration Updates" menu item. Look in:

- Help menu → Check for Configuration Updates
- Or use the commented-out menu item code in `GUI_App.cpp` line 6490

**6. Verify the Dialog Appears:**

After restarting, the configuration update dialog should appear automatically on startup if an update is detected. The dialog will show:

- The heading: "A new configuration package available, Do you want to install it?"
- The changelog content in the scrollable area (this is what needs word wrapping)

**Note:** If you want to test with long changelog text similar to the screenshot, you may need to modify the `BBL.changelog` file or the cached changelog content to include longer lines that would overflow without proper wrapping.