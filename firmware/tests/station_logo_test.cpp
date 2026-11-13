#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/StationLogoFormat.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace pocketjazz::logo;

struct MemoryReader {
  const std::vector<uint8_t>& data;
  size_t pos = 0;
  size_t maxChunk = 9999;
  size_t size() const { return data.size(); }
  size_t read(uint8_t* out, size_t n) {
    if (pos >= data.size()) return 0;
    size_t count = n < maxChunk ? n : maxChunk;
    if (count > data.size() - pos) count = data.size() - pos;
    std::memcpy(out, data.data() + pos, count);
    pos += count;
    return count;
  }
};

static std::vector<uint8_t> fixture() {
  std::vector<uint8_t> result(kFileBytes, 0);
  result[0]='P'; result[1]='J'; result[2]='L'; result[3]='G';
  result[4]=kVersion; result[5]=kWidth; result[6]=kHeight;
  result[7]=kRgb565Le; result[8]=0; result[9]=0x20;
  for (size_t i=0; i<kPixelsBytes; ++i)
    result[kHeaderBytes+i] = static_cast<uint8_t>((i * 17u) & 0xffu);
  const uint32_t crc=crc32(result.data()+kHeaderBytes,kPixelsBytes);
  for (unsigned i=0;i<4;++i) result[12+i]=static_cast<uint8_t>(crc >> (8*i));
  return result;
}

int main() {
  assert(stationUrlHash("") == UINT64_C(0xcbf29ce484222325));
  assert(stationUrlHash("abc") == UINT64_C(0xe71fa2190541574b));
  static_assert(kPixelsBytes == 8192 && kHeaderBytes == 16 && kFileBytes == 8208,
                "v1 logo file must remain strictly bounded");
  const auto base = fixture();
  assert(validHeader(base.data(), kHeaderBytes));
  assert(validate(base.data(), base.size()));
  assert(crc32(reinterpret_cast<const uint8_t*>("123456789"),9) == 0xcbf43926u);
  std::vector<uint8_t> output(kPixelsBytes);
  MemoryReader good{base};
  assert(load(good, output.data(), output.size()));
  assert(good.pos == base.size());
  assert(std::memcmp(output.data(),base.data()+kHeaderBytes,kPixelsBytes)==0);
  assert(pixel(output.data(), 0) == uint16_t(base[16] | (uint16_t(base[17])<<8)));

  for (int i=0;i<16;++i) {
    auto tampered=base;
    tampered[i]^=1;
    assert(!validate(tampered.data(),tampered.size()));
    MemoryReader reader{tampered};
    assert(!load(reader, output.data(), output.size()));
  }
  auto corrupted=base; corrupted[777]^=0x40;
  assert(!validate(corrupted.data(),corrupted.size()));
  MemoryReader badCrc{corrupted};
  assert(!load(badCrc,output.data(),output.size()));
  for (size_t size : {size_t(0),size_t(1),size_t(16),size_t(8207),size_t(8209)}) {
    auto wrong=base; wrong.resize(size);
    assert(!validate(wrong.data(),wrong.size()));
    MemoryReader reader{wrong};
    assert(!load(reader,output.data(),output.size()));
  }
  MemoryReader truncatedIo{base}; truncatedIo.maxChunk=128;
  assert(!load(truncatedIo,output.data(),output.size()));
  MemoryReader tooSmall{base};
  assert(!load(tooSmall,output.data(),kPixelsBytes-1));
  MemoryReader noCache{base};
  assert(!load(noCache,nullptr,kPixelsBytes));
  return 0;
}
