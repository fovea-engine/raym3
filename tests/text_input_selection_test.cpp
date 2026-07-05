#include "raym3/v2/TextInput.h"

#include <cassert>

using raym3::v2::TextInputDraggedEdge;
using raym3::v2::TextInputResolveDraggedSelection;

int main() {
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::Start, 8, 3, 12);
    assert(update.applied);
    assert(update.selectionStart == 3);
    assert(update.selectionEnd == 8);
    assert(update.cursor == 3);
  }
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::Start, 8, 8, 12);
    assert(!update.applied);
  }
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::Start, 8, 10, 12);
    assert(!update.applied);
  }
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::End, 3, 8, 12);
    assert(update.applied);
    assert(update.selectionStart == 3);
    assert(update.selectionEnd == 8);
    assert(update.cursor == 8);
  }
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::End, 3, 3, 12);
    assert(!update.applied);
  }
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::End, 3, 1, 12);
    assert(!update.applied);
  }
  {
    auto update =
        TextInputResolveDraggedSelection(TextInputDraggedEdge::End, -3, 30, 12);
    assert(update.applied);
    assert(update.selectionStart == 0);
    assert(update.selectionEnd == 12);
    assert(update.cursor == 12);
  }

  return 0;
}
