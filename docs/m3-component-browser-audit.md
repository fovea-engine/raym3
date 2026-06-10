# M3 Component Browser Audit

Generated from the Browser plugin against https://m3.material.io/components on 2026-05-28. Use this as the implementation checklist for raym3 v2 component behavior and animation.

## Motion Baseline

- M3 Expressive component motion is moving to a physics/spring system. The site describes two schemes: expressive, which can overshoot final values for bounce, and standard, which is more utilitarian with minimal bounce.
- Component motion should be token-driven. The motion overview calls out two component motion tokens: expressive fast spatial and expressive fast effects.
- Legacy transition defaults remain useful for screen/view transitions: emphasized 500ms, emphasized decelerate 400ms enter, emphasized accelerate 200ms exit, standard 300ms, standard decelerate 250ms enter, and standard accelerate 200ms exit.
- Transitions should prioritize container transform, shared/persistent elements, enter/exit, fade, scale, and platform-appropriate horizontal navigation motion.

## Component Checklist

### Button groups

- Source: https://m3.material.io/components/button-groups/overview and /specs
- Site summary: Button groups; Button groups organize buttons and add interactions between them; info; style
- Behavior/motion notes: Selection & activation; States; M3 Expressive; Default shape; Selection; Single-select, multi-select, selection-required
- Visual animation/spec media: A standard button group and a segmented button group.; Standard button group in 3 of 5 available sizes, and segmented button group with just icon buttons and just common buttons.; Various colors and shapes of standard and connected button groups.; Five sizes of button groups and two shapes of button groups.; The container outlined on both variants of button groups.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Buttons

- Source: https://m3.material.io/components/buttons/overview and /specs
- Site summary: Buttons; Buttons prompt most actions in a UI; info; style
- Behavior/motion notes: States; Shape morph; M3 Expressive; Toggle (selection); Shape; Toggle: unselected
- Visual animation/spec media: 5 variants of buttons.; 4 button changes in the expressive update.; Rectangular M2 buttons.; Round-cornered M3 buttons.; Diagram comparing buttons with toggle buttons.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Extended FABs

- Source: https://m3.material.io/components/extended-fab/overview and /specs
- Site summary: Extended FABs; info; style; design_services
- Behavior/motion notes: M3 Expressive update; Shape: Boxier style with smaller corner radius; M2: Extended FABs are pill-shaped and have a different height and elevation; M3: Extended FABs share the same height, boxier shape, and simpler elevation model as FABs; Use an extended FAB on screens with long, scrolling views that require persistent access to an action, such as a checkout screen.; Use an extended FAB to provide constant access to a primary action above long-scrolling surface content
- Visual animation/spec media: 3 extended fab sizes.; The baseline extended FAB and the small, medium, and large extended FABs from the expressive update.; Diagram comparing the M2 FAB and extended FAB.; Diagram comparing the M3 FAB and extended FAB.; Vibrant extended FAB on an email screen.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### FAB menu

- Source: https://m3.material.io/components/fab-menu/overview and /specs
- Behavior/motion notes: The floating action button (FAB) menu opens from a FAB to display multiple related actions; States; M3 Expressive; Use the table's menu to switch token sets. The FAB menu has a common token set and six color sets, three for each element (close button and menu item). Learn about design tokens; Close button; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states
- Visual animation/spec media: The FAB menu in its single variant.; 3 color configurations of FAB menus.; 2 elements of a FAB menu.; 5 FAB menus showing the range of 2–6 items.; 12 colors of the FAB menu.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Floating action buttons (FABs)

- Source: https://m3.material.io/components/floating-action-button/overview and /specs
- Site summary: FABs; info; style; design_services
- Behavior/motion notes: FABs persist on the screen when content is scrolling; M3 Expressive update; The FAB has new sizes to match the extended FAB and more color options. The small FAB is no longer recommended. More on M3 Expressive; M3: FABs have a boxier shape, can use dynamic color, and include a new large FAB variation; The FAB can be aligned left, center, or right. It can be positioned above the navigation bar, or nested within it.; A large FAB is useful in any window size when the layout calls for a clear and prominent primary action, but is best suited for expanded and larger window sizes, where its size helps draw attention.
- Visual animation/spec media: The 3 sizes of floating action buttons.; 4 FABs showing the colors available after the expressive update.; M2 circular FAB with a plus icon.; M3 rounded corner square FAB with an artist’s palette icon.; 3 screens with various FAB sizes.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Icon buttons

- Source: https://m3.material.io/components/icon-buttons/overview and /specs
- Site summary: Icon buttons; Icon buttons help people take minor actions with one tap; info; style
- Behavior/motion notes: States; Shape morph; M3 Expressive; Toggle (selection); Two shapes; Shape
- Visual animation/spec media: 5 kinds of outline buttons.; Icon buttons can vary in size, shape, and width.; Icon buttons were known as toggle buttons in M2.; Side by side view of default and toggle icon buttons.; Side by side view of size, shape, color, and width variations.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Segmented buttons

- Source: https://m3.material.io/components/segmented-buttons/overview and /specs
- Behavior/motion notes: States; Icon (optional for unselected state); States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Unselected; Unselected button states:; Hovered
- Visual animation/spec media: Diagram of segmented button indicating 3 parts of its anatomy.; Diagram of segmented button indicating its color mappings; Side by side view of segmented buttons with 5 unselected states.; Side by side view of segmented buttons with 4 selected states.; Diagram indicating layout values, paddings, and target size for segmented buttons
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Split buttons

- Source: https://m3.material.io/components/split-button/overview and /specs
- Site summary: Split buttons; Split buttons open a menu to give people more options related to an action; info; style
- Behavior/motion notes: Split buttons open a menu to give people more options related to an action; States; M3 Expressive; Split buttons use the same color schemes as standard buttons. However, unlike toggle buttons, the split button color doesn’t change when selected—only a state layer is applied.; Split buttons use the same colors and state layers as buttons, shown in the following token module. Go to buttons for more details.; A: Unselected, B: Selected trailing icon
- Visual animation/spec media: pause Split buttons are made of a common button and a menu icon button; An extra large split button. It has a label and icon on one part of the button, and a menu icon on the other part.; 5 sizes of split buttons.; 1 type of split button.; 4 colors and 5 sizes of split buttons.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Date pickers

- Source: https://m3.material.io/components/date-pickers/overview and /specs
- Site summary: Date pickers; Date pickers let people select a date, or a range of dates; info; style
- Behavior/motion notes: Menu button: Month selection; Menu button: Year selection; Unselected date; Selected date; Menu button: Month selection (pressed); Menu button: Year selection (disabled)
- Visual animation/spec media: 3 variants of date pickers side-by-side. The docked date picker has an outlined text field above a calendar view. The modal date picker allows people to select a date from a calendar view. The modal date input lets someone type in a date.; Old version of a date picker with a white background and shadows.; New version of date picker with a colorful background, rounded corners, and no shadows.; Diagram indicating the 11 elements of a docked date picker.; Diagram indicating 8 elements of a docked date picker with an open dropdown menu showing the months May to November.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Time pickers

- Source: https://m3.material.io/components/time-pickers/overview and /specs
- Site summary: Time pickers; info; style; design_services
- Behavior/motion notes: States; Clock dial selector center; Hover; Focus; Pressed; States specs can be found in the token module above
- Visual animation/spec media: Dial time picker dial and input time picker.; Time picker’s old color mappings. The selected hour of 7 and AM text is purple, on a purple background.; Time picker's new color mappings. The selected hour of 7 and AM text is black, with different background colors.; Diagram indicating the 14 elements of a time picker dial.; Diagram indicating the 10 elements of a time picker input.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Loading indicator

- Source: https://m3.material.io/components/loading-indicator/overview and /specs
- Behavior/motion notes: Loading indicator; Loading indicators show the progress for a short wait time; Loading indicators are best for indicating a short, indeterminate wait time; Loading indicators use animation to grab attention, mitigate perceived latency, and indicate that an activity is in progress.; While similar in function to circular progress indicators, loading indicators are a better alternative for short processes between 200ms and 5s.; Use a loading indicator when a background process is running
- Visual animation/spec media: pause Loading indicators are best for indicating a short, indeterminate wait time; Loading indicator on media player.; pause Use a loading indicator when a background process is running; Loading indicator in loading state with “Getting your device ready...”.; pause Instant (under 200ms): Display the content immediately
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Progress indicators

- Source: https://m3.material.io/components/progress-indicators/overview and /specs
- Site summary: Progress indicators; info; style; design_services
- Behavior/motion notes: Progress indicators; Linear progress indicator; Circular progress indicator; M3 Expressive; Shape: Flat and wavy; Shape
- Visual animation/spec media: 8 progress indicators configured to show different thickness and shape.; pause Progress indicators have a new rounded, colorful style, and more configurations to choose from, including a wavy shape and variable track height; Progress indicators used when loading a page and for processing a payment.; GM3 linear and circular progress indicators; M2 linear and circular progress indicators.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Navigation bar

- Source: https://m3.material.io/components/navigation-bar/overview and /specs
- Behavior/motion notes: Initial focus; Visual indicators; When a navigation item is tapped, the active indicator appears in place, providing feedback that it’s selected; A touch ripple passes through the indicator; When hovered, the active indicator appears in a reduced state providing a visual cue that the destination is interactive; When clicked (in both active and inactive states), a ripple passes through the indicator
- Visual animation/spec media: pause Touch: Tap; On a navigation bar, when the Home and Explore icons are tapped, an active indicator is displayed as interaction feedback.; pause Cursor: Hover, Click; On a navigation bar, the hover and click interactions on the Home and Explore icons have different interaction feedback.; Nav bar with text scaled to 1.5x size. Some labels are on two lines, others are on one line.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Navigation drawer

- Source: https://m3.material.io/components/navigation-drawer/overview and /specs
- Site summary: Navigation drawer; Navigation drawers let people switch between UI views on larger devices; info; style
- Behavior/motion notes: States; Active indicator; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Navigation drawer states:; Hovered; Focused
- Visual animation/spec media: 2 variants of navigation drawers: standard and modal.; M2 navigation drawer with 4 destinations in a mail app. The active destination “Inbox” is rectangular.; M3 navigation drawer with 4 destinations in a mail app. The active destination “Inbox” has rounded corners.; Navigation drawer diagram numbering 7 elements; Navigation drawer diagram numbering 9 color roles.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Navigation rail

- Source: https://m3.material.io/components/navigation-rail/overview and /specs
- Site summary: Navigation rail; Navigation rails let people switch between UI views on mid-sized devices; info; style
- Behavior/motion notes: States; Collapsed navigation rail; Expanded navigation rail; The baseline navigation rail is no longer recommended, and should be replaced by the collapsed navigation rail. View baseline tokens; M3 Expressive; Use collapsed navigation rail.
- Visual animation/spec media: pause Collapsed and expanded navigation rails can transition between each other on any device, including: 1. Large or medium window size classes like tablets 2. Compact window size classes like phones in portrait orientation; Navigation rail with 4 destinations, 1 active, and FAB.; A collapsed and expanded navigation rail.; M2 navigation rail with 1 colored and filled icon showing the active state and 3 inactive icons.; M3 navigation rail with 1 icon surrounded by a pill shape in contrasting color to show the active state.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Bottom sheets

- Source: https://m3.material.io/components/bottom-sheets/overview and /specs
- Site summary: Bottom sheets; Bottom sheets show secondary content anchored to the bottom of the screen; info; style
- Behavior/motion notes: Bottom sheets show secondary content anchored to the bottom of the screen; Drag handle (optional); Drag handle alignment (horizontal); Center; Drag handle padding top/bottom; Shape: Bottom sheets have a 28dp top corner radius
- Visual animation/spec media: Side by side view of standard bottom sheet modal bottom sheet; Diagram of floating sheet set on screen background; Diagram of container, drag handle, scrim; Two diagrams featuring color opposites of scrim, container, drag handle; Bottom sheet on larger device with 56dp top and 56dp side margins
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Side sheets

- Source: https://m3.material.io/components/side-sheets/overview and /specs
- Site summary: Side sheets; Side sheets show secondary content anchored to the side of the screen; info; style
- Behavior/motion notes: Side sheets show secondary content anchored to the side of the screen; Close icon button; Shape: Modal side sheets have a 16dp corner radius; Standard side sheets are supplementary surfaces used mostly in medium to expanded window sizes, like tablet and desktop. They provide a consistent and predictable surface for contextual actions and information.; close; Don’t inset a side sheet from the screen edges far beyond the recommended margin. This makes the sheet’s position and scroll behavior unclear, while obscuring primary content.
- Visual animation/spec media: The 2 variants of side sheets.; A modal side sheet showing the 16dp corner radius.; 4 elements of a standard side sheet.; 4 color roles applied to a side sheet in light and dark themes.; Standard side sheet padding and size measurements.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### App bars

- Source: https://m3.material.io/components/app-bars/overview and /specs
- Site summary: App bars; App bars are placed at the top of the screen to help people navigate through a product; info; style
- Behavior/motion notes: M3 Expressive; Center-aligned; Use centered-text configuration.; Text labels, including supporting text, can be aligned to the leading edge or centered; Centered; The app bar can have different layouts depending on which elements are shown
- Visual animation/spec media: 4 configurations of app bars stacked vertically to show differences.; 4 total app bar configurations.; M2 top app bar with elevation to separate it from main content.; M3 app bar with subtle color difference from main content.; 4 variants of app bars.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Badges

- Source: https://m3.material.io/components/badges/overview and /specs
- Site summary: Badges; Badges show notifications, counts, or status information on navigation items and icons; info; style
- Behavior/motion notes: Badges show notifications, counts, or status information on navigation items and icons; Small badge shape; Large badge shape; Different badges are shown on navigation destinations in various states.; In navigation bars, hide the badge once the destination has been selected; A small badge uses only shape to indicate a status change or new notification
- Visual animation/spec media: 3 icons with badges. 1 is a small dot. 2 is a larger circle with a 1 digit number. 3 is an oval with a 4 digit number.; Navigation bar showing 4 icons with different badge variants in a bright red color.; 5 aspects of badge anatomy on a navigation bar.; 5 aspects of badge anatomy on a navigation rail.; 5 applications of badge color on light and dark theme navigation bars.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Cards

- Source: https://m3.material.io/components/cards/overview and /specs
- Behavior/motion notes: Elevated card states; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Elevated card states:; Hovered; Focused; Pressed
- Visual animation/spec media: Diagram indicating elevated card container.; Color diagram indicating elevated card surface color.; Diagram of 5 elevated card states.; Diagram indicating filled card container.; Color diagram indicating filled card surface color.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Carousel

- Source: https://m3.material.io/components/carousel/overview and /specs
- Behavior/motion notes: Carousel; Carousels show a collection of items that can be scrolled on and off the screen; States; Carousel item dynamic widths; Center-aligned hero; Large carousel item
- Visual animation/spec media: 4 elements of a carousel.; 2 color roles of a carousel.; 5 states of a carousel in light and dark schemes.; Measurements for a small carousel item.; 4 elements of a multi-browse carousel layout.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Checkbox

- Source: https://m3.material.io/components/checkbox/overview and /specs
- Site summary: Checkbox; Checkboxes let users select one or more items from a list, or turn an item on or off; info; style
- Behavior/motion notes: States; State-layer; The text color remains the same regardless if the checkbox is selected or not; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Hovered; Focused
- Visual animation/spec media: 3 checkboxes in a diagram demonstrating all three states.; Color mapping of a checkbox in M2.; Color mapping of a checkbox in M3 with new color.; Diagram of checkbox indicating the 2 parts of its anatomy.; Checkbox color roles in light and dark themes.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Chips

- Source: https://m3.material.io/components/chips/overview and /specs
- Site summary: Chips; Chips help people enter information, make selections, filter content, or trigger actions; info; style
- Behavior/motion notes: Chips help people enter information, make selections, filter content, or trigger actions; Assist chip states; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Selected and unselected assist chip states:; Hovered; Focused
- Visual animation/spec media: 4 chip variants.; A chip with a clear outline is now a chip with a subtle outline.; M2 chip variants.; M3 chip variants.; Assist chip diagram numbering 3 elements.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Dialogs

- Source: https://m3.material.io/components/dialogs/overview and /specs
- Site summary: Dialogs; Dialogs provide important prompts in a user flow; info; style
- Behavior/motion notes: Container shape; Center-aligned; Icon (close affordance); Icon (close affordance) size; Shape: Increased corner-radius; New updates to color, layout, position, shape, and typography
- Visual animation/spec media: Basic and full-screen dialog.; Basic dialog with rounded corner, larger headline.; Anatomy diagram numbering dialog elements.; Color mapping diagram labeling 6 color roles across the dialog and scrim.; Annotated diagram showing padding values.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Divider

- Source: https://m3.material.io/components/divider/overview and /specs
- Site summary: Divider; Dividers are thin lines that group content in lists or other containers; info; style
- Behavior/motion notes: Only use dividers if items can’t be grouped with open space; Inset dividers can be placed in the middle of a layout to separate elements such as body text from selection chips
- Visual animation/spec media: Screen shot of five stacked dividers; Screen shot of three dividers; Diagram of divider set on horizontal line; Divider on light background and dark background.; Divider's measurement.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Lists

- Source: https://m3.material.io/components/lists/overview and /specs
- Site summary: Lists; info; style; design_services
- Behavior/motion notes: Expressive lists; Use the expressive list variant for more flexible styling, highlighted selection states, and customizable slots.; An expressive list has a segmented style and round corners; In M3 Expressive, baseline lists are still available to use, but don’t have the latest visual style, selection treatment, and slot functionality.; M3 Expressive; List (expressive)
- Visual animation/spec media: 1 list contains 3 items, each with a label text, supporting text, and trailing text. A music app shows list items with leading images.; 2 party planning lists with 2 completed list items each. In 1 list, the selected items are highlighted.; 3 variants of lists in M2.; 3 variants of lists in M3 baseline.; 2 expressive lists: a photos list on a tablet, and a song list on mobile.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Menus

- Source: https://m3.material.io/components/menus/overview and /specs
- Behavior/motion notes: States; Use vertical menus for a more expressive look and feel, including rounded corners, standard and vibrant color styles, more selection states, and submenu motion.; In M3 Expressive, baseline menu is still available to use, but doesn’t have the latest shapes, color styles, selection states, and motion. See baseline menu specs; A baseline menu has square corners, as compared to a vertical menu’s round corners and expressive styling; M3 Expressive; On surface (state layer)
- Visual animation/spec media: 2 vertical menus use shape and color to indicate selected state.; A baseline menu variant with square corners and standard colors.; 2 menus: 1 standard, and 1 with a gap, creating groups.; A diagram of a vertical menu.; 2 vertical menus: 1 with lower visual emphasis, and 1 vibrant menu with bold shades.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Radio button

- Source: https://m3.material.io/components/radio-button/overview and /specs
- Site summary: Radio button; Radio buttons let people select one option from a set of options; info; style
- Behavior/motion notes: States; The text color remains the same regardless if the button is selected or not; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Hover; Focus; Pressed
- Visual animation/spec media: 1 radio button is selected from a list of 4 radio buttons of different ringtones.; App screen with 1 active button selected from list of 3 buttons.; Diagram of enabled radio button.; Diagram of selected and unselected radio button colors.; Radio buttons with labels. The labels are the same color for both selected and unselected radio buttons.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Search

- Source: https://m3.material.io/components/search/overview and /specs
- Site summary: Search; Search lets people enter a keyword or phrase to get relevant information; info; style
- Behavior/motion notes: Search lets people enter a keyword or phrase to get relevant information; M3 Expressive update; Search has a new visual style, motion, and more flexibility for trailing icons. More on M3 Expressive; Motion; The search bar grows wider when focused; Name: Search was formerly known as open search bar
- Visual animation/spec media: pause When inputting text, search suggestions or results appear below the search bar; Mobile UI shows a person typing into an email search bar. It expands to show a list of results.; pause The contained search style features a persistent, filled search container; A recipe search with “Search recipes” hinted text, “Mexican dishes” is entered, then results appear in a list.; M2 open search bar.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Sliders

- Source: https://m3.material.io/components/sliders/overview and /specs
- Site summary: Sliders; Sliders let users make selections from a range of values; info; style
- Behavior/motion notes: Sliders; Sliders let users make selections from a range of values; States; Centered; M3 Expressive; Available as “continuous” slider
- Visual animation/spec media: pause Sliders change values along a range; A vertical slider changes the brightness of bedroom lights.; 3 M3 Expressive sliders.; M3 visually-refreshed slider.; M2 slider.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Snackbar

- Source: https://m3.material.io/components/snackbar/overview and /specs
- Site summary: Snackbar; Snackbars show short updates about app processes at the bottom of the screen; info; style
- Behavior/motion notes: Snackbars show short updates about app processes at the bottom of the screen; Icon (optional close affordance); Sliders: Overview; Dialogs are also designed to show important messages.; Dialog High priority Required: Dialogs block app usage until the user takes a dialog action or exits the dialog (if available); Close button (optional)
- Visual animation/spec media: Diagram of snackbar placement; Example of snackbar on screen bottom; Diagram of snackbar indicating the four parts of its anatomy; Diagram of snackbar indicating color and inverse text and labels; Diagram of snackbar with action
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Switch

- Source: https://m3.material.io/components/switch/overview and /specs
- Site summary: Switch; Switches toggle the selection of an item on or off; info; style
- Behavior/motion notes: Switches toggle the selection of an item on or off; States; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Hovered; Focused; Pressed
- Visual animation/spec media: A switch in two states, off and on.; M2 switches in off and on states.; M3 switch shown toggled off and toggled on. When switched on, it has a checkmark icon.; 3 elements of a switch.; 6 color roles of a switch in light and dark themes.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Tabs

- Source: https://m3.material.io/components/tabs/overview and /specs
- Site summary: Tabs; Tabs organize content across different screens and views; info; style
- Behavior/motion notes: Active indicator; Primary tabs states; Hover (active destination); Focused (active destination); Pressed (active destination); Hover (inactive destination)
- Visual animation/spec media: A bar of primary tabs with destinations labeled Flights, Trips, and Explore. And a bar of secondary tabs with destinations labeled Overview and Specifications; Bar of primary tabs with destinations labeled Flights, Trips, and Explore; 6 elements of primary tabs.; 7 color roles applied to primary tabs in light and dark themes.; Diagram of all primary tab states in both light and dark mode
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Text fields

- Source: https://m3.material.io/components/text-fields/overview and /specs
- Site summary: Text fields; Text fields let users enter text into a UI; info; style
- Behavior/motion notes: Text fields let users enter text into a UI; Focused active Indicator; Enabled active indicator; Filled text field states; States are visual representations used to communicate the status of a component or interactive element. Learn more about interaction states; Focused (empty)
- Visual animation/spec media: 2 variants of text fields, filled and outlined.; A filled and outlined text field with M3 color mappings.; Diagram of a filled text field indicating the 10 parts of its anatomy.; Diagram of a filled text field indicating its color mappings.; Side by side view of empty and populated filled text fields across different states, showing the differences between enabled, focused, hovered, and disabled.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Toolbars

- Source: https://m3.material.io/components/toolbars/overview and /specs
- Site summary: Toolbars; Toolbars display frequently used actions relevant to the current page; info; style
- Behavior/motion notes: M3 Expressive; By default all toolbars are 64dp high, center-aligned, have equal padding between items, and have a minimum outside padding of 16dp.; Center-aligned, 8dp padding between items; M3 Expressive update; Two expressive variants: docked toolbar and floating toolbar; Don’t show at the same time as a navigation bar
- Visual animation/spec media: 2 variants of toolbars.; 2 examples of toolbar variants.; M2 bottom app bar.; M3 bottom app bar.; Baseline bottom app bar, which looks like the docked toolbar, but is not recommended.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

### Tooltips

- Source: https://m3.material.io/components/tooltips/overview and /specs
- Site summary: Tooltips; Tooltips display brief labels or messages; info; style
- Behavior/motion notes: Shape: Rich tooltips have more rounded corners; close; Don't hide critical information within tooltips as it’s easy to miss. Use an interruptive dialog instead.; On desktop, tooltips may appear centered below the parent element and remain visible while moving within the target region.; To show a tooltip, hover on the parent element on desktop, or tap and hold the element on mobile. Persistent rich tooltips only appear when clicked or tapped.; Triggering a new tooltip immediately closes any other open tooltip.
- Visual animation/spec media: 2 variants of tooltips.; GM2 rich tooltip.; GM3 rich tooltip.; 2 elements of a plain tooltip.; 2 color roles of a plain tooltip.
- raym3 v2 target: add/verify tokens, states, keyboard/pointer behavior, layout constraints, and motion transitions for this component.

