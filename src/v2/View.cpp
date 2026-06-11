#include "raym3/v2/View.h"

#include "raym3/styles/Theme.h"

#include <cstring>
#include <utility>

namespace raym3::v2 {

Node::Node(NodeKind nodeKind) : kind(nodeKind) {}

static void ApplyViewProps(Node &node, const ViewProps &props) {
  node.id = props.id;
  node.style = props.style;
  node.stateStyles = props.stateStyles;
  node.motion = props.motion;
  node.disabled = props.disabled;
  node.capturesInput = props.capturesInput;
  node.zIndex = props.zIndex;
  node.anchorRect = props.anchorRect;
  node.placement = props.placement;
  node.onPress = props.onPress;
}

NodePtr View(const ViewProps &props, std::vector<NodePtr> children) {
  auto node = std::make_shared<Node>(NodeKind::View);
  ApplyViewProps(*node, props);
  node->children = std::move(children);
  return node;
}

NodePtr Text(std::string text, const TextProps &props) {
  auto node = std::make_shared<Node>(NodeKind::Text);
  node->id = props.id;
  node->style = props.style;
  if (!node->style.text.fontSize)
    node->style.text.fontSize = Theme::GetTypographyScale().bodyMedium;
  if (!node->style.text.lineHeight)
    node->style.text.lineHeight = 20.0f;
  if (!node->style.text.letterSpacing)
    node->style.text.letterSpacing = 0.25f;
  if (!node->style.text.color)
    node->style.text.color = Theme::GetColorScheme().onSurface;
  node->stateStyles = props.stateStyles;
  node->motion = props.motion;
  node->disabled = props.disabled;
  node->text = std::move(text);
  return node;
}

NodePtr TextInput(const TextInputProps &props) {
  auto node = std::make_shared<Node>(NodeKind::TextInput);
  node->id = props.id;
  node->style = props.style;
  node->stateStyles = props.stateStyles;
  node->motion = props.motion;
  node->disabled = props.disabled;
  node->textInput = props;
  node->textInput.passwordMode = props.passwordMode || props.secure;
  if (props.value) {
    node->inputScratch = *props.value;
    node->inputBuffer.assign(1024, '\0');
    std::strncpy(node->inputBuffer.data(), props.value->c_str(),
                 node->inputBuffer.size() - 1);
  } else if (props.buffer) {
    node->inputScratch = props.buffer;
  }
  return node;
}

NodePtr Button(const ButtonProps &props, std::vector<NodePtr> children) {
  auto node = std::make_shared<Node>(NodeKind::Button);
  node->id = props.id;
  node->style = props.style;
  node->stateStyles = props.stateStyles;
  node->motion = props.motion;
  node->disabled = props.disabled;
  node->onPress = props.onPress;
  node->inkRipple = true;
  node->text = props.label;
  node->buttonVariant = props.variant;
  node->children = std::move(children);
  return node;
}

NodePtr Custom(const ViewProps &props, std::function<void(Rectangle)> render,
               std::vector<NodePtr> children) {
  auto node = std::make_shared<Node>(NodeKind::Custom);
  ApplyViewProps(*node, props);
  node->customRender = std::move(render);
  node->children = std::move(children);
  return node;
}

} // namespace raym3::v2
