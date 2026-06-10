#pragma once

#include "raym3/v2/View.h"

namespace raym3::v2 {

enum class M3Component {
  AppBar,
  Badge,
  Banner,
  BottomAppBar,
  BottomSheet,
  DataTable,
  DockedToolbar,
  FloatingToolbar,
  ButtonGroup,
  Button,
  Card,
  Carousel,
  Checkbox,
  Chip,
  DatePicker,
  Dialog,
  Divider,
  ExtendedFab,
  Fab,
  FabMenu,
  IconButton,
  List,
  LoadingIndicator,
  Menu,
  MenuItem,
  NavigationBar,
  NavigationBarItem,
  NavigationDrawer,
  NavigationRail,
  ProgressIndicator,
  RadioButton,
  RangeSlider,
  Search,
  SegmentedButton,
  SideSheet,
  Slider,
  Snackbar,
  SplitButton,
  Switch,
  Tabs,
  TextField,
  TimePicker,
  Toolbar,
  Tooltip,
  Popover
};

enum class NavigationItemLayout { Default, Column, Row };

struct ComponentProps {
  std::string id;
  std::string label;
  Style style;
  StateStyles stateStyles;
  MotionSpec motion;
  bool disabled = false;
  bool selected = false;
  bool checked = false;
  bool indeterminate = false;
  bool wavy = false;
  bool open = false;
  bool scrim = false;
  bool capturesInput = false;
  NavigationItemLayout navigationItemLayout = NavigationItemLayout::Default;
  int zIndex = 0;
  float progress = -1.0f;
  float startProgress = -1.0f;
  float endProgress = -1.0f;
  float wavelength = -1.0f;
  int anchorId = 0;
  PopoverPlacement placement = PopoverPlacement::Auto;
  std::function<void()> onPress;
};

struct ComponentSpec {
  M3Component component;
  const char *name;
  const char *sourcePath;
  MotionSpec motion;
  bool interactive;
  bool selectable;
  bool overlay;
};

ComponentSpec GetComponentSpec(M3Component component);
NodePtr MaterialComponent(M3Component component, const ComponentProps &props,
                          std::vector<NodePtr> children = {});

} // namespace raym3::v2
