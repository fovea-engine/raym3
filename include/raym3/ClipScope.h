#pragma once

#include <raylib.h>
#include <vector>

namespace raym3 {

void PushClipRect(Rectangle bounds);
void PopClipRect();
Rectangle GetCurrentClipRect();
bool HasActiveClipRect();

bool IsRectInClip(Rectangle bounds);
bool IsPointInClip(Vector2 point);

void ApplyClipRectToGpu(Rectangle rect);
void ClearClipStack();

void SuspendClipScissor();
void ResumeClipScissor();

void SetClipDebug(bool enabled);
bool IsClipDebug();
void GetClipDebugRects(std::vector<Rectangle> &out);
void ClearClipDebugRects();
int GetClipDepth();
int GetClipUnderflowCount();
void ClearClipUnderflowCount();

} // namespace raym3
