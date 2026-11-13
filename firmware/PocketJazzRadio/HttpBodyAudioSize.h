// SPDX-License-Identifier: GPL-3.0-or-later
// A finite HTTP Content-Length includes ICY metadata; decoder getPos() does
// NOT. Track raw entity payload separately so temporary gaps before EOF can
// wait, while the actual end (even with metadata) is recognized immediately.
#ifndef POCKETJAZZ_HTTP_BODY_AUDIO_SIZE_H
#define POCKETJAZZ_HTTP_BODY_AUDIO_SIZE_H
#include <stdint.h>

namespace pocketjazz {
inline uint32_t projectedMp3Size(int32_t httpContentLength,
                                 uint32_t bodyBytesConsumed,
                                 uint32_t mp3BytesDelivered,
                                 uint32_t unknownSize) {
  if (httpContentLength < 0) return unknownSize;
  const uint32_t totalBody = static_cast<uint32_t>(httpContentLength);
  // Source has counted all actual entity bytes, including ICY length byte
  // and metadata, but never HTTP chunk framing. This is a projected decoded
  // audio end, updated as metadata arrives; it never changes getPos().
  const uint32_t bodyRemaining = bodyBytesConsumed >= totalBody
                                   ? 0u : totalBody - bodyBytesConsumed;
  return mp3BytesDelivered + bodyRemaining;
}
}  // namespace pocketjazz
#endif  // POCKETJAZZ_HTTP_BODY_AUDIO_SIZE_H
