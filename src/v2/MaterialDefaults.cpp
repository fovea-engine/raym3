#include "raym3/v2/MaterialDefaults.h"

#include "raym3/v2/MaterialTokens.h"
#include <algorithm>

namespace raym3::v2 {

using namespace tokens;

MaterialComponentMetrics GetMaterialMetrics(M3Component component) {
  MaterialComponentMetrics m;
  switch (component) {
  case M3Component::AppBar:
  case M3Component::Toolbar:
    m.layoutHeight = kAppBarHeight;
    m.minHeight = kAppBarHeight;
    break;
  case M3Component::BottomAppBar:
    m.layoutHeight = kBottomAppBarHeight;
    m.minHeight = kBottomAppBarHeight;
    break;
  case M3Component::Button:
    m.layoutHeight = kButtonHeight;
    m.minWidth = kButtonMinWidth;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::IconButton:
    m.layoutWidth = kIconButtonSize;
    m.layoutHeight = kIconButtonSize;
    m.touchWidth = kMinimumTouchTarget;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::Fab:
    m.layoutWidth = kFabSize;
    m.layoutHeight = kFabSize;
    m.visualWidth = kFabSize;
    m.visualHeight = kFabSize;
    break;
  case M3Component::ExtendedFab:
    m.layoutHeight = kFabSize;
    m.minWidth = kExtendedFabMinWidth;
    break;
  case M3Component::Checkbox:
    m.layoutWidth = kCheckboxLayoutSize;
    m.layoutHeight = kCheckboxLayoutSize;
    m.minWidth = kCheckboxLayoutSize;
    m.minHeight = kCheckboxLayoutSize;
    m.visualWidth = kCheckboxVisualSize;
    m.visualHeight = kCheckboxVisualSize;
    m.touchWidth = kMinimumTouchTarget;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::RadioButton:
    m.layoutWidth = kRadioLayoutSize;
    m.layoutHeight = kRadioLayoutSize;
    m.minWidth = kRadioLayoutSize;
    m.minHeight = kRadioLayoutSize;
    m.visualWidth = kRadioVisualSize;
    m.visualHeight = kRadioVisualSize;
    m.touchWidth = kMinimumTouchTarget;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::Switch:
    m.layoutWidth = kSwitchLayoutWidth;
    m.layoutHeight = kSwitchLayoutHeight;
    m.minWidth = kSwitchLayoutWidth;
    m.minHeight = kSwitchLayoutHeight;
    m.visualWidth = kSwitchTrackWidth;
    m.visualHeight = kSwitchTrackHeight;
    m.touchWidth = kMinimumTouchTarget;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::Slider:
  case M3Component::RangeSlider:
    m.layoutHeight = kSliderTouchTargetHeight;
    m.minWidth = kSliderMinWidth;
    m.visualHeight = kSliderTrackHeight;
    m.touchHeight = kSliderTouchTargetHeight;
    break;
  case M3Component::ProgressIndicator:
    m.layoutHeight = kLinearProgressTrackHeight;
    m.minWidth = kLinearProgressMinWidth;
    m.visualHeight = kLinearProgressTrackHeight;
    break;
  case M3Component::LoadingIndicator:
    m.layoutWidth = kProgressIndicatorSize;
    m.layoutHeight = kProgressIndicatorSize;
    m.visualWidth = kProgressIndicatorSize;
    m.visualHeight = kProgressIndicatorSize;
    break;
  case M3Component::TextField:
    m.layoutHeight = kTextFieldHeight;
    m.minWidth = kTextFieldMinWidth;
    break;
  case M3Component::Search:
    m.layoutHeight = kSearchBarHeight;
    break;
  case M3Component::NavigationBar:
    m.layoutHeight = kNavigationBarHeight;
    break;
  case M3Component::NavigationBarItem:
    m.layoutHeight = kNavigationBarItemHeight;
    m.minWidth = kNavigationBarItemMinWidth;
    break;
  case M3Component::NavigationRail:
    m.layoutWidth = kNavigationRailWidth;
    break;
  case M3Component::NavigationDrawer:
    m.layoutWidth = kNavigationDrawerWidth;
    break;
  case M3Component::BottomSheet:
  case M3Component::Dialog:
  case M3Component::DatePicker:
  case M3Component::TimePicker:
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::SideSheet:
    m.layoutWidth = kSideSheetWidth;
    break;
  case M3Component::Badge:
    m.layoutHeight = kBadgeLargeHeight;
    m.minWidth = kBadgeLargeMinWidth;
    break;
  case M3Component::Chip:
    m.layoutHeight = kChipHeight;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::SegmentedButton:
  case M3Component::ButtonGroup:
  case M3Component::SplitButton:
    m.layoutHeight = kSegmentedButtonHeight;
    m.touchHeight = kMinimumTouchTarget;
    break;
  case M3Component::Tabs:
    m.layoutHeight = kTabsHeight;
    break;
  case M3Component::Snackbar:
    m.minHeight = kSnackbarMinHeight;
    break;
  case M3Component::Menu:
  case M3Component::MenuItem:
  case M3Component::Popover:
  case M3Component::FabMenu:
  case M3Component::List:
  case M3Component::Card:
  case M3Component::Carousel:
  case M3Component::Divider:
  case M3Component::Banner:
  case M3Component::DataTable:
  case M3Component::DockedToolbar:
  case M3Component::FloatingToolbar:
  case M3Component::Tooltip:
    break;
  }
  return m;
}

MaterialReference GetMaterialReference(M3Component component) {
  switch (component) {
  case M3Component::Button:
  case M3Component::ButtonGroup:
  case M3Component::SegmentedButton:
  case M3Component::SplitButton:
    return {"button_style_button.dart / filled_button.dart / segmented_button.dart", "tokenized"};
  case M3Component::IconButton:
    return {"icon_button.dart", "tokenized"};
  case M3Component::Fab:
  case M3Component::ExtendedFab:
    return {"floating_action_button.dart", "tokenized"};
  case M3Component::Checkbox:
    return {"checkbox.dart", "tokenized visual/tap target"};
  case M3Component::RadioButton:
    return {"radio.dart", "tokenized visual/tap target"};
  case M3Component::Switch:
    return {"switch.dart", "tokenized visual/tap target"};
  case M3Component::Slider:
  case M3Component::RangeSlider:
    return {"slider.dart / slider_theme.dart / slider_parts.dart", "tokenized 2024 M3"};
  case M3Component::ProgressIndicator:
  case M3Component::LoadingIndicator:
    return {"progress_indicator.dart", "tokenized"};
  case M3Component::TextField:
    return {"input_decorator.dart / text_field.dart", "tokenized"};
  case M3Component::Search:
    return {"search_anchor.dart", "tokenized"};
  case M3Component::NavigationBar:
  case M3Component::NavigationBarItem:
    return {"navigation_bar.dart", "tokenized"};
  case M3Component::NavigationRail:
    return {"navigation_rail.dart", "tokenized"};
  case M3Component::NavigationDrawer:
    return {"navigation_drawer.dart", "tokenized"};
  case M3Component::Tabs:
    return {"tabs.dart", "tokenized"};
  case M3Component::AppBar:
  case M3Component::BottomAppBar:
  case M3Component::Toolbar:
  case M3Component::DockedToolbar:
  case M3Component::FloatingToolbar:
    return {"app_bar.dart / bottom_app_bar.dart", "tokenized"};
  case M3Component::Card:
    return {"card.dart", "tokenized"};
  case M3Component::Dialog:
    return {"dialog.dart", "tokenized"};
  case M3Component::BottomSheet:
  case M3Component::SideSheet:
    return {"bottom_sheet.dart", "tokenized"};
  case M3Component::Menu:
  case M3Component::MenuItem:
  case M3Component::Popover:
  case M3Component::FabMenu:
    return {"menu_anchor.dart / menu_style.dart", "tokenized"};
  case M3Component::Snackbar:
    return {"snack_bar.dart", "tokenized"};
  case M3Component::Tooltip:
    return {"tooltip.dart", "tokenized"};
  case M3Component::Badge:
    return {"badge.dart", "tokenized"};
  case M3Component::Chip:
    return {"chip.dart / action_chip.dart / filter_chip.dart", "tokenized"};
  case M3Component::Divider:
    return {"divider.dart", "tokenized"};
  case M3Component::List:
    return {"list_tile.dart / list_tile_theme.dart", "tokenized"};
  case M3Component::Banner:
    return {"banner.dart", "tokenized"};
  case M3Component::DataTable:
    return {"data_table.dart", "placeholder tokenized"};
  case M3Component::Carousel:
    return {"carousel.dart", "placeholder tokenized"};
  case M3Component::DatePicker:
    return {"date_picker.dart / date_picker_theme.dart", "placeholder tokenized"};
  case M3Component::TimePicker:
    return {"time_picker.dart / time_picker_theme.dart", "placeholder tokenized"};
  }
  return {"material.dart", "unknown"};
}

Rectangle CenteredVisualRect(Rectangle layout, float visualWidth,
                             float visualHeight) {
  float w = visualWidth > 0.0f ? std::min(visualWidth, layout.width) : layout.width;
  float h = visualHeight > 0.0f ? std::min(visualHeight, layout.height) : layout.height;
  return {layout.x + (layout.width - w) * 0.5f,
          layout.y + (layout.height - h) * 0.5f, w, h};
}

Rectangle MinimumTouchRect(Rectangle visual, float minWidth, float minHeight) {
  float w = std::max(visual.width, minWidth);
  float h = std::max(visual.height, minHeight);
  return {visual.x + (visual.width - w) * 0.5f,
          visual.y + (visual.height - h) * 0.5f, w, h};
}

} // namespace raym3::v2
