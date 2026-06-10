#include "raym3/v2/RenderContext.h"

namespace raym3::v2 {

static RenderContext s_defaultContext;
static RenderContext *s_currentContext = &s_defaultContext;

RenderContext &Ctx() { return *s_currentContext; }

void SetCurrentRenderContext(RenderContext *ctx) {
  s_currentContext = ctx ? ctx : &s_defaultContext;
}

} // namespace raym3::v2
