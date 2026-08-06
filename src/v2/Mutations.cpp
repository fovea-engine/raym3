#include "raym3/Mutations.h"

#include "raym3/v2/Components.h"
#include "raym3/v2/Renderer.h"

#include <algorithm>
#include <unordered_map>

namespace raym3 {

void ApplyMutations(v2::RenderContext &ctx, MutationBatch &batch,
                    std::map<int, v2::NodePtr> &nodes,
                    v2::NodePtr &root) {
  (void)ctx;
  // Lazily built on the first DisposeNode in this batch; see that case.
  std::unordered_map<const v2::Node *, v2::Node *> parentOf;
  bool parentIndexBuilt = false;
  for (const Mutation &m : batch.ops) {
    switch (m.op) {
    case MutationOp::CreateView: {
      v2::ViewProps props;
      props.style = m.style;
      props.zIndex = m.zIndex;
      props.capturesInput = m.capturesInput;
      nodes[(int)m.id] = v2::View(props);
      break;
    }
    case MutationOp::CreateText: {
      v2::TextProps props;
      props.style = m.style;
      nodes[(int)m.id] = v2::Text(m.text.c_str(), props);
      break;
    }
    case MutationOp::CreateMaterial: {
      v2::ComponentProps props;
      props.label = m.label;
      props.style = m.style;
      nodes[(int)m.id] = v2::MaterialComponent(static_cast<v2::M3Component>(m.componentType),
                                            props, {});
      break;
    }
    case MutationOp::DisposeNode: {
      // Unlink from the parent before dropping the map's reference. Erasing
      // alone only gives up *this* map's share of the node: if a parent still
      // holds it in `children` it stays alive, keeps being laid out, painted and
      // hit-tested, and no longer has an id anyone can address it by.
      //
      // The parent index is built at most once per batch, not once per dispose —
      // scanning every node for each disposed node would make tearing down a
      // large tree quadratic.
      auto it = nodes.find((int)m.id);
      if (it != nodes.end()) {
        const v2::NodePtr doomed = it->second;
        if (!parentIndexBuilt) {
          for (auto &[otherId, candidate] : nodes) {
            (void)otherId;
            if (!candidate) continue;
            for (const v2::NodePtr &child : candidate->children)
              if (child) parentOf[child.get()] = candidate.get();
          }
          if (root)
            for (const v2::NodePtr &child : root->children)
              if (child) parentOf[child.get()] = root.get();
          parentIndexBuilt = true;
        }
        auto pit = parentOf.find(doomed.get());
        if (pit != parentOf.end() && pit->second) {
          auto &children = pit->second->children;
          children.erase(std::remove(children.begin(), children.end(), doomed),
                         children.end());
          parentOf.erase(pit);
        }
        if (root == doomed) root.reset();
        nodes.erase(it);
      }
      break;
    }
    case MutationOp::SetRoot:
      if (auto it = nodes.find((int)m.id); it != nodes.end()) root = it->second;
      break;
    case MutationOp::ClearRoot:
      root.reset();
      break;
    case MutationOp::AppendChild: {
      auto pit = nodes.find((int)m.parentId);
      auto cit = nodes.find((int)m.childId);
      if (pit != nodes.end() && cit != nodes.end()) {
        // A re-append is a move-to-end: drop any existing occurrence first so
        // the child can't sit in the vector twice (double YGNodeInsertChild
        // aborts the Yoga build).
        auto &children = pit->second->children;
        children.erase(std::remove(children.begin(), children.end(), cit->second),
                       children.end());
        children.push_back(cit->second);
      }
      break;
    }
    case MutationOp::RemoveChild: {
      auto pit = nodes.find((int)m.parentId);
      auto cit = nodes.find((int)m.childId);
      if (pit != nodes.end() && cit != nodes.end()) {
        auto &children = pit->second->children;
        children.erase(std::remove(children.begin(), children.end(), cit->second),
                       children.end());
      }
      break;
    }
    case MutationOp::InsertBefore: {
      auto pit = nodes.find((int)m.parentId);
      auto cit = nodes.find((int)m.childId);
      auto bit = nodes.find((int)m.beforeId);
      if (pit == nodes.end() || cit == nodes.end()) break;
      auto &children = pit->second->children;
      children.erase(std::remove(children.begin(), children.end(), cit->second),
                     children.end());
      if (bit == nodes.end()) {
        children.push_back(cit->second);
      } else {
        auto beforeIt = std::find(children.begin(), children.end(), bit->second);
        children.insert(beforeIt, cit->second);
      }
      break;
    }
    case MutationOp::SetStyle: {
      if (auto it = nodes.find((int)m.id); it != nodes.end())
        it->second->style = m.style;
      break;
    }
    case MutationOp::SetText: {
      if (auto it = nodes.find((int)m.id); it != nodes.end())
        it->second->text = m.text;
      break;
    }
    case MutationOp::SetValue: {
      if (auto it = nodes.find((int)m.id); it != nodes.end())
        it->second->control.value = m.value;
      break;
    }
    case MutationOp::SetLayout: {
      if (auto it = nodes.find((int)m.id); it != nodes.end()) {
        it->second->layout.x = m.layoutX;
        it->second->layout.y = m.layoutY;
        it->second->layout.width = m.layoutW;
        it->second->layout.height = m.layoutH;
      }
      break;
    }
    case MutationOp::SetScrollOffset: {
      // NOTE: unlike JS_setScrollOffset this clears neither the in-flight fling
      // nor scrollFollowEnd, so the write is reverted by the next fling tick.
      // Traced here; funnelled through ScrollTo() in the scroll-core rework.
      if (auto it = nodes.find((int)m.id); it != nodes.end()) {
        v2::ScrollTraceOffsetWrite(*it->second,
                                   v2::ScrollWriteSource::Mutation, 'y',
                                   it->second->scrollOffsetY, m.scrollOffsetY);
        it->second->scrollOffsetY = m.scrollOffsetY;
      }
      break;
    }
    }
  }
  batch.ops.clear();
}

} // namespace raym3
