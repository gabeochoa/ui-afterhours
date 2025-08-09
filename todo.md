## UI Afterhours Showcase Backlog

This is a big, categorized backlog of ideas to build out the example app and comprehensively demo the Afterhours UI plugin and related systems. Use it as a menu; check off in any order.

- add support for command line arguments to navigate the UI so we can automate checking this is working 

### What’s already shown in `src/main.cpp`
- [x] Basic ECS bootstrapping, window, systems wiring
- [x] Immediate UI root container via `div`
- [x] Core widgets: `button`, `checkbox`, `slider`, `dropdown`
- [x] Input mapping for `WidgetNext`, `WidgetBack`, `WidgetPress`, value up/down

## Core UI components and patterns
- [ ] Button group: showcase `button_group` (horizontal and vertical)
- [ ] Checkbox group: use `checkbox_group` with min/max selection caps
- [ ] Pagination: demo `pagination` with several pages and keyboard nav
- [ ] Navigation bar: demo `navigation_bar` with left/right arrows and center label
- [ ] Icon row: use `icon_row` with a spritesheet to present selectable icons
- [ ] Image and sprite: render textures via `.with_texture` and `sprite`
- [ ] Compound controls: label + control rows (e.g., label + slider with different corner treatments)
- [ ] Select-on-focus behavior: demonstrate `.with_select_on_focus(true)`
- [ ] Skip-from-tabbing behavior: demonstrate `.with_skip_tabbing(true)`
- [ ] Disabled/hidden states: show `.with_disabled(true)` and `.with_hidden(true)` effects

## Layout and sizing (AutoLayout)
- [ ] Flex direction: nested `div` containers with `.with_flex_direction(Row|Column)`
- [ ] Sizing: combine `pixels(...)`, `percent(...)`, and `children(...)` to create responsive layouts
- [ ] Padding vs margin: visual demo of spacing using `Padding` and `Margin`
- [ ] Absolute positioning: `.with_absolute_position()` for overlays/badges
- [ ] Render layers: use `.with_render_layer(n)` to verify z-ordering and focus
- [ ] Overflow/scroll demo: stress test with many children; ensure clipping is correct
- [ ] Responsive container: resize window and show layout adjustments across breakpoints

## Theming and styling
- [ ] UIStylingDefaults: populate defaults in a dedicated setup system using `UIStylingDefaults::set_component_config` per `ComponentType`
- [ ] Color usages: `Theme::Usage::{Primary,Secondary,Accent,Error,Background}` and `.with_custom_color(...)`
- [ ] Rounded corners: use `RoundedCorners` helpers (`left_round`, `right_sharp`, etc.) and mix-and-match per child
- [ ] Font/typography: load fonts with `font_helper.h` and use `.with_font(name,size)` per component
- [ ] Theme switcher: runtime toggle between Light/Dark/custom themes; apply to active context
- [ ] Disabled color treatment: verify `Theme::from_usage(..., disabled=true)` darkens correctly

## Input, focus, and navigation
- [ ] Keyboard navigation: robust demo of `WidgetNext`/`WidgetBack`, including Shift+Tab behavior using `WidgetMod`
- [ ] Gamepad navigation: map DPAD/shoulders to `WidgetNext`/`WidgetBack` and `WidgetPress`
- [ ] Value change via keyboard: `ValueUp`/`ValueDown` for sliders/pagination
- [ ] Focus ring polish for checkboxes: address noted TODO so focus aligns with actual clickable area
- [ ] Close dropdown on blur: implement the TODO to close when leaving without selection

## Data-driven UI with providers
- [ ] Dropdown with provider: implement a provider component and wire `HasDropdownStateWithProvider` (`providers.h`)
- [ ] Window resolution provider: feed options from `window_manager::ProvidesAvailableWindowResolutions`
- [ ] Dynamic option updates: demonstrate options changing at runtime and UI refreshing (`UpdateDropdownOptions` system)
- [ ] Write-back to provider: call `on_data_changed(index)` on selection

## Composite demos (end-to-end examples)
- [ ] Settings panel: toggles (checkbox group), volume (slider), theme select (dropdown), apply/reset buttons
- [ ] Audio mixer: multiple labeled sliders with left/right rounded segments and live value display
- [ ] Toolbar with icon sprites: `icon_row` with hover/focus states and active selection
- [ ] Pagination + list: switch pages, show a paged grid of items
- [ ] Navigation bar + views: change content view via `navigation_bar` selection
- [ ] Modal dialog: absolute positioned overlay + centered dialog container; confirm/cancel buttons
- [ ] Toast/notification: absolute top-right container with timed auto-hide
- [ ] Tooltip on hover/focus: lightweight info box anchored to element (absolute positioning)

## Visual polish and rendering
- [ ] Layered UI: show multiple overlapping components with different `.with_render_layer` values
- [ ] Texture backgrounds: support background textures via `.with_texture(...)` alignment options
- [ ] Sprite sheet selection: clickable sprite atlas preview using `texture_manager`
- [ ] Error states: use `Theme::Usage::Error` for validation errors on input widgets

## Performance, stability, and UX quality
- [ ] Stress test: thousands of simple `div`/`button` elements to profile autolayout and input processing
- [ ] Entity reuse: demonstrate `imm::mk(parent, index)` for stable element identity across frames; warn on source-location reuse pitfalls
- [ ] Frame-time HUD: small overlay (already shows FPS) with counts of entities, UI elements, draw calls
- [ ] Memory churn audit: track entity alloc/free during UI creation; ensure minimal churn
- [ ] Edge cases: dropdown with zero options (warn path), very long labels, extremely small/large sizes

## Documentation and developer experience
- [ ] In-code docs: brief comments above composite demo setup systems describing intent and key API calls
- [ ] README: add animated screenshots/gifs of each demo section
- [ ] Example gallery: split demos into togglable scenes (e.g., via a `dropdown`/`navigation_bar` for selecting demo)
- [ ] Keyboard mapping table: list currently mapped keys and allow remapping at runtime

## Refactors and internal improvements
- [ ] Fill out `SetupUIStylingDefaults`: provide real defaults for Button, Checkbox, Slider, Dropdown, etc.
- [ ] Extract demo UI systems into separate files under `src/` (e.g., `ui_demo_*`)
- [ ] Address focus/active state inconsistencies in checkboxes (see inline TODOs in `imm_components.h`)
- [ ] Add a small utility to close open dropdowns when clicking outside or changing focus

---

### Nice-to-haves and stretch goals
- [ ] Theme animator: smooth transitions when switching themes (colors/rounded corners)
- [ ] Grid/list virtualization: windowed rendering for huge lists to keep FPS stable
- [ ] Tabs and split panes: build from existing primitives (button group + `div` containers)
- [ ] Tree view: hierarchical collapsible list using `div` nesting
- [ ] Progress bar and spinner: built from `div` + autolayout
- [ ] Color picker: palette built using `icon_row` + custom colors

### File/API references for quick linking
- Components (immediate): `vendor/afterhours/src/plugins/ui/immediate/imm_components.h`
- Config and defaults: `vendor/afterhours/src/plugins/ui/immediate/component_config.h`
- Rounded corners: `vendor/afterhours/src/plugins/ui/immediate/rounded_corners.h`
- Context and input: `vendor/afterhours/src/plugins/ui/context.h`, UI systems: `vendor/afterhours/src/plugins/ui/systems.h`
- Providers: `vendor/afterhours/src/plugins/ui/providers.h`
- Theme: `vendor/afterhours/src/plugins/ui/theme.h`

