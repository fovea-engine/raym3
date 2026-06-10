#pragma once

#include "raym3/v2/Input.h"
#include "raym3/v2/View.h"
#include <raylib.h>

namespace raym3::v2 {

void SpawnRipple(NodeId owner, Vector2 origin, Rectangle bounds, Color inkColor);
void StartRippleFadeOut(NodeId owner);
void CancelRipplesForNode(NodeId owner);
void TickRipples(float dt);
bool HasRipplesForNode(NodeId owner);
bool HasActiveRipples();
void PaintRipplesForNode(const Node &node, Rectangle bounds, float cornerRadius);

} // namespace raym3::v2
