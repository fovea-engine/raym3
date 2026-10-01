#include "raym3/v2/RenderContext.h"

namespace raym3::v2 {

static RenderContext s_defaultContext;
static thread_local RenderContext *s_currentContext = &s_defaultContext;

RenderContext &Ctx() { return *s_currentContext; }

void SetCurrentRenderContext(RenderContext *ctx) {
  s_currentContext = ctx ? ctx : &s_defaultContext;
}

RenderContext *GetCurrentRenderContext() {
  return s_currentContext == &s_defaultContext ? nullptr : s_currentContext;
}

} // namespace raym3::v2
