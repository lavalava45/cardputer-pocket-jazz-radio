#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
inline uint32_t& fakeClock() { static uint32_t t = 0; return t; }
inline uint32_t millis() { return fakeClock(); }
inline uint32_t micros() { return fakeClock() * 1000; }
inline std::function<void()>& delayHook() { static std::function<void()> h; return h; }
inline void vTaskDelay(int ticks) { fakeClock() += ticks; if (delayHook()) delayHook()(); }
namespace m5 {
class Speaker_Class {
 public:
  bool accept = true;
  const void* pending[3]{};
  unsigned requests = 0;
  void* context = nullptr;
  void (*callback)(void*, const void*, uint8_t) = nullptr;
  void setBufferReleaseCallback(void* c, void (*fn)(void*,const void*,uint8_t)) { context=c; callback=fn; }
  bool playRaw(const int16_t* data, size_t, uint32_t, bool, int, uint8_t) {
    if (!accept) return false;
    pending[requests++ % 3] = data; return true;
  }
  void release() {
    for (auto& data : pending) if (data) { callback(context,data,0); data=nullptr; }
  }
};
}
