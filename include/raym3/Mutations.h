#pragma once

#include "raym3/v2/RenderContext.h"
#include "raym3/v2/Style.h"
#include "raym3/v2/View.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace raym3 {

enum class MutationOp : uint8_t {
  CreateView = 1,
  CreateText,
  CreateMaterial,
  DisposeNode,
  SetRoot,
  ClearRoot,
  AppendChild,
  RemoveChild,
  InsertBefore,
  SetStyle,
  SetText,
  SetValue,
  SetLayout,
  SetScrollOffset,
};

struct Mutation {
  MutationOp op = MutationOp::CreateView;
  uint32_t id = 0;
  uint32_t parentId = 0;
  uint32_t childId = 0;
  uint32_t beforeId = 0;
  uint8_t componentType = 0;
  int zIndex = 0;
  bool capturesInput = false;
  float value = 0.0f;
  float layoutX = 0.0f;
  float layoutY = 0.0f;
  float layoutW = 0.0f;
  float layoutH = 0.0f;
  float scrollOffsetY = 0.0f;
  raym3::v2::Style style{};
  std::string text;
  std::string label;
};

struct MutationBatch {
  uint64_t epoch = 0;
  std::vector<Mutation> ops;
};

void ApplyMutations(v2::RenderContext &ctx, MutationBatch &batch,
                    std::map<int, v2::NodePtr> &nodes,
                    v2::NodePtr &root);

} // namespace raym3
