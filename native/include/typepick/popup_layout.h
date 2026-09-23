// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <windows.h>
#include <algorithm>
#include <optional>

namespace typepick {
inline RECT RecommendationRect(RECT caret, RECT work, std::optional<RECT> candidates = {}) {
  const LONG width = (std::min)(420L, work.right - work.left);
  const LONG height = (std::min)(40L, work.bottom - work.top);
  auto at = [&](LONG x, LONG y) { return RECT{x, y, x + width, y + height}; };
  auto fits = [&](RECT r) { return r.left >= work.left && r.top >= work.top &&
      r.right <= work.right && r.bottom <= work.bottom; };
  if (candidates) {
    const RECT c = *candidates;
    for (RECT r : {at(c.right + 8, c.top), at(c.left - width - 8, c.top),
                   at(c.left, c.top - height - 8), at(c.left, c.bottom + 8)})
      if (fits(r)) return r;
  }
  // Older installed TSF builds cannot report their window class. Prefer above
  // the caret instead of the old fixed offset inside the ordinary candidate list.
  LONG x = (std::clamp)(caret.left, work.left, work.right - width);
  LONG y = caret.top - height - 8;
  if (y < work.top) {
    x = work.right - width;
    y = (std::clamp)(caret.bottom + 8, work.top, work.bottom - height);
  }
  return at(x, (std::clamp)(y, work.top, work.bottom - height));
}
}
