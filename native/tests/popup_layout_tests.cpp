// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/popup_layout.h>
#include <stdexcept>
int main() {
  RECT work{0, 0, 1920, 1080};
  for (RECT candidates : {RECT{215, 85, 377, 380}, RECT{1800, 80, 1920, 350}, RECT{900, 880, 1100, 1060}}) {
    RECT caret{candidates.left, candidates.top-20, candidates.left, candidates.top-4};
    RECT result = typepick::RecommendationRect(caret, work, candidates), overlap{};
    if (IntersectRect(&overlap, &result, &candidates) || result.left < 0 || result.top < 0 ||
        result.right > 1920 || result.bottom > 1080) throw std::runtime_error("popup overlaps candidates or leaves screen");
  }
  RECT caret{215, 64, 215, 80};
  if (typepick::RecommendationRect(caret, work).bottom >= caret.top)
    throw std::runtime_error("legacy fallback must be above caret");
}
