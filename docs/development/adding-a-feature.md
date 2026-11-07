# Adding a Feature

A feature is one of the sections you swipe up and down between, like Images or Level.  Each one lives in its own `core/ui/feature_<name>.c`, and [`core/ui/feature_level.c`](../../core/ui/feature_level.c) is a small, complete example to start from.

1. Create `core/ui/feature_<name>.c` with a `ui_feature_def_t` called `feature_<name>` (see [`core/ui/ui_internal.h`](../../core/ui/ui_internal.h)):
   - `build` adds the feature's pages with `ui_feature_add_page()`.  Return `false` to leave the feature out, for example when the board doesn't have the hardware it needs (see [The hardware abstraction layer](hardware-abstraction.md)).
   - `on_navigate` is optional and is called whenever a different page comes on screen.  Use it to start and stop anything that only needs to run while the feature is showing.
   - Size things with `ui_px()`, which scales from the 240x240 layout to the board's screen.
   - `ui_add_startup_choice()` lets a page be picked in the Startup setting.
2. Add the name to `RCAT_FEATURES` in [`core/features.cmake`](../../core/features.cmake).
3. Add it to the list in [`core/ui/ui.c`](../../core/ui/ui.c), inside `#if RCAT_FEATURE_<NAME>`, above Settings.  Settings stays last so it can list every feature's startup choices.
4. Add a line for it to each platform's `features.cmake`, `ON` or `OFF`.
5. Add a page for it in [`docs/features/`](../features/), and a row in the table in its [README](../features/README.md) and in the [top-level README](../../README.md#what-it-does).

## Settings and build options

- **Something people change on the display** goes in `settings_t` in [`core/app/settings.h`](../../core/app/settings.h).  Add new fields only at the end, so settings saved by older firmware still load.  Give the field a default in `set_defaults()`, check it in `load()`, and add a setter in [`core/app/settings.c`](../../core/app/settings.c).  Settings that belong to one feature go on that feature's own pages, not in the Settings feature.
- **Something fixed for a build**, like which speaker the ESP32-S3-Touch-LCD-1.46 uses, goes in the platform's `features.cmake`, or in the board's `.cmake` file if it depends on the board.  The build passes it to the code as a compile definition.

## Things that run in the background

Some features keep things going while you look at others: the sound player keeps playing, and the Lovense devices stay connected.  Put that part in `core/app/` or `core/audio/`, and keep the feature's file to its pages.  Do regular work in an LVGL timer (`lv_timer_create()`), created in the feature's `build`, which keeps running whichever page is on screen.  The [phone remote](../features/phone-remote.md) calls the same modules, so it and the display always agree.
