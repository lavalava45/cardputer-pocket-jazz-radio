// SPDX-License-Identifier: MIT
// Portable, allocation-free, bounded reader for Pocket Jazz MusicBrainz notes.
// No Arduino/SD dependency: caller owns the files and implements random read.
#ifndef POCKET_JAZZ_NOTES_SEARCH_H
#define POCKET_JAZZ_NOTES_SEARCH_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pocketjazz {

struct NotesFile {
  void* context;
  size_t size;
  // Must fill exactly n bytes and return true, or return false.
  bool (*read)(void* context, size_t offset, uint8_t* dst, size_t n);
};

struct JazzNote {
  char key[49];
  char description[109];
  char album[111];
  char artistUrl[101];
  char albumUrl[111];
  char attribution[91];
  char license[41];
};

class JazzNotesSearch {
 public:
  enum { MaxRows = 50, MaxTsvBytes = 32768, MaxIdxBytes = 4096 };
  JazzNotesSearch() : tsv_(), count_(0), indexed_(false), ready_(false) {}

  // A malformed IDX is ignored: valid TSV remains searchable by full scan.
  // Invalid TSV, too many rows or failed reads fail closed.
  bool open(NotesFile tsv, NotesFile idx = NotesFile()) {
    ready_ = indexed_ = false;
    count_ = 0;
    tsv_ = tsv;
    if (!tsv.read || !tsv.size || tsv.size > MaxTsvBytes) return false;
    Cursor cursor(tsv);
    char line[614];
    size_t offset = 0, used = 0;
    JazzNote note;
    while (nextLine(cursor, line, sizeof(line), used, offset)) {
      if (count_ == MaxRows || !decode(line, used, note)) return false;
      for (size_t i = 0; i < count_; ++i)
        if (strcmp(index_[i].key, note.key) == 0) return false;
      index_[count_].hash = fnv(note.key);
      index_[count_].offset = static_cast<uint32_t>(offset);
      memcpy(index_[count_].key, note.key, strlen(note.key) + 1);
      ++count_;
    }
    if (!cursor.ok() || count_ == 0 || cursor.position() != tsv.size) return false;
    ready_ = true;
    if (idx.read && idx.size && idx.size <= MaxIdxBytes) indexed_ = loadIndex(idx);
    return true;
  }

  bool ready() const { return ready_; }
  bool indexed() const { return ready_ && indexed_; }
  size_t rows() const { return ready_ ? count_ : 0; }

  // Exact lowercase ASCII slug from artist_key, e.g. "miles-davis".
  bool lookup(const char* key, JazzNote& out) const {
    if (!ready_ || !validKey(key)) return false;
    if (indexed_) {
      const uint32_t hash = fnv(key);
      size_t lo = 0, hi = count_;
      while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (index_[mid].hash < hash) lo = mid + 1;
        else hi = mid;
      }
      for (size_t i = lo; i < count_ && index_[i].hash == hash; ++i) {
        if (strcmp(index_[i].key, key) == 0) {
          return readRecord(index_[i].offset, out) &&
                 strcmp(out.key, key) == 0;
        }
      }
      return false;
    }
    Cursor cursor(tsv_);
    char line[614];
    size_t used = 0, offset = 0;
    JazzNote note;
    while (nextLine(cursor, line, sizeof(line), used, offset)) {
      if (!decode(line, used, note)) return false;
      if (strcmp(note.key, key) == 0) { out = note; return true; }
    }
    return false;
  }

  // ASCII case-insensitive substring of artist slug OR album title.
  // Stable TSV order; returns matches copied into caller storage (no heap).
  // Truncated results are explicitly signaled via totalMatches if supplied.
  size_t search(const char* text, JazzNote* out, size_t capacity,
                size_t* totalMatches = nullptr, size_t skip = 0) const {
    if (totalMatches) *totalMatches = 0;
    if (!ready_ || !text || !*text || (!out && capacity)) return 0;
    size_t queryLen = 0;
    while (text[queryLen] && queryLen <= 110) {
      const uint8_t c = static_cast<uint8_t>(text[queryLen]);
      if (c < 32 || c > 126) return 0;
      ++queryLen;
    }
    if (queryLen == 0 || queryLen > 110) return 0;
    char slugQuery[111];
    for (size_t i = 0; i < queryLen; ++i)
      slugQuery[i] = text[i] == ' ' ? '-' : text[i];
    slugQuery[queryLen] = 0;
    Cursor cursor(tsv_);
    char line[614];
    size_t offset = 0, used = 0, found = 0, stored = 0;
    JazzNote note;
    while (nextLine(cursor, line, sizeof(line), used, offset)) {
      if (!decode(line, used, note)) return 0;
      if (contains(note.key, slugQuery) || contains(note.album, text)) {
        if (found >= skip && stored < capacity) out[stored++] = note;
        ++found;
      }
    }
    if (!cursor.ok()) return 0;
    if (totalMatches) *totalMatches = found;
    return stored;
  }

  static uint32_t fnv(const char* key) {
    uint32_t h = 2166136261UL;
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(key);
         *c; ++c) h = (h ^ *c) * 16777619UL;
    return h;
  }

 private:
  struct IndexEntry { uint32_t hash; uint32_t offset; char key[49]; };
  NotesFile tsv_;
  IndexEntry index_[MaxRows];
  size_t count_;
  bool indexed_, ready_;

  class Cursor {
   public:
    explicit Cursor(NotesFile file, size_t initialPosition = 0)
        : file_(file), pos_(initialPosition), start_(0), length_(0),
          good_(initialPosition <= file.size) {}
    bool byte(uint8_t& c) {
      if (!good_ || pos_ == file_.size) return false;
      if (pos_ < start_ || pos_ >= start_ + length_) {
        start_ = pos_;
        length_ = file_.size - start_;
        if (length_ > sizeof(cache_)) length_ = sizeof(cache_);
        if (!file_.read(file_.context, start_, cache_, length_)) {
          good_ = false;
          return false;
        }
      }
      c = cache_[pos_++ - start_];
      return true;
    }
    bool ok() const { return good_; }
    size_t position() const { return pos_; }
    void invalidate() { good_ = false; }
   private:
    NotesFile file_;
    size_t pos_, start_, length_;
    uint8_t cache_[128];
    bool good_;
  };

  static bool nextLine(Cursor& c, char* dst, size_t cap,
                       size_t& length, size_t& offset) {
    offset = c.position();
    length = 0;
    uint8_t byte = 0;
    while (c.byte(byte)) {
      if (byte == '\n') {
        if (length == 0) { c.invalidate(); return false; }
        dst[length] = '\0';
        return true;
      }
      if (byte == '\r' || byte >= 127 ||
          (byte < 32 && byte != '\t') || length + 1 >= cap) {
        c.invalidate();
        return false;
      }
      dst[length++] = static_cast<char>(byte);
    }
    if (length) c.invalidate(); // Missing mandatory terminal LF.
    return false;
  }

  static bool validKey(const char* key) {
    if (!key || !*key) return false;
    size_t n = 0;
    bool dash = false;
    for (; key[n]; ++n) {
      if (n >= 48) return false;
      const char c = key[n];
      if (c == '-') {
        if (!n || dash) return false;
        dash = true;
      } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        dash = false;
      } else return false;
    }
    return !dash;
  }

  static bool decode(char* line, size_t length, JazzNote& out) {
    char* fields[7] = {};
    fields[0] = line;
    size_t field = 0;
    for (size_t i = 0; i < length; ++i) {
      if (line[i] == '\t') {
        if (field == 6) return false;
        line[i] = '\0';
        fields[++field] = &line[i + 1];
      }
    }
    if (field != 6 || !validKey(fields[0])) return false;
    const size_t limits[] = {48, 108, 110, 100, 110, 90, 40};
    char* output[] = {out.key, out.description, out.album, out.artistUrl,
                      out.albumUrl, out.attribution, out.license};
    for (size_t i = 0; i < 7; ++i) {
      const size_t n = strlen(fields[i]);
      if (!n || n > limits[i]) return false;
      memcpy(output[i], fields[i], n + 1);
    }
    return true;
  }

  bool readRecord(size_t offset, JazzNote& out) const {
    if (offset >= tsv_.size || !tsv_.read) return false;
    // Start a cached reader at the exact index offset. SD callbacks typically
    // read 128 bytes at once, never one physical read for every character.
    Cursor cursor(tsv_,offset);
    char line[614];
    size_t n = 0, start = 0;
    return nextLine(cursor, line, sizeof(line), n, start) &&
           decode(line, n, out);
  }

  static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
  }
  static bool uintField(const char* text, uint32_t& value, int base) {
    if (!*text) return false;
    uint32_t v = 0;
    for (const char* p = text; *p; ++p) {
      int digit = base == 16 ? hex(*p) :
                  (*p >= '0' && *p <= '9' ? *p - '0' : -1);
      if (digit < 0 || digit >= base || v > (UINT32_MAX - digit) / base)
        return false;
      v = v * base + static_cast<uint32_t>(digit);
    }
    value = v;
    return true;
  }
  static bool contains(const char* haystack, const char* needle) {
    for (size_t i = 0; haystack[i]; ++i) {
      size_t n = 0;
      while (needle[n] && haystack[i + n]) {
        char a = haystack[i + n], b = needle[n];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) break;
        ++n;
      }
      if (!needle[n]) return true;
    }
    return false;
  }

  // SHA-256 is deliberately implemented locally so the same strict check
  // works on desktop and ESP32 without heap or external crypto dependencies.
  struct Sha256 {
    uint32_t h[8], w[64];
    uint8_t block[64];
    size_t filled;
    uint64_t bytes;
    Sha256() : filled(0), bytes(0) {
      const uint32_t init[] = {0x6a09e667UL,0xbb67ae85UL,0x3c6ef372UL,
        0xa54ff53aUL,0x510e527fUL,0x9b05688cUL,0x1f83d9abUL,0x5be0cd19UL};
      memcpy(h, init, sizeof(h));
    }
    static uint32_t rot(uint32_t x, unsigned n) {
      return (x >> n) | (x << (32 - n));
    }
    void compress() {
      static const uint32_t k[64] = {
        0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,
        0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
        0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,
        0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
        0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,
        0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
        0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,
        0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
        0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,
        0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
        0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,
        0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
        0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,
        0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
        0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,
        0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL
      };
      for (unsigned i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
               (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
      }
      for (unsigned i = 16; i < 64; ++i) {
        const uint32_t a = w[i-15], b = w[i-2];
        w[i] = w[i-16] + (rot(a,7)^rot(a,18)^(a>>3)) + w[i-7] +
               (rot(b,17)^rot(b,19)^(b>>10));
      }
      uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
      for (unsigned i = 0; i < 64; ++i) {
        const uint32_t t1=v+(rot(e,6)^rot(e,11)^rot(e,25))+
          ((e&f)^(~e&g))+k[i]+w[i];
        const uint32_t t2=(rot(a,2)^rot(a,13)^rot(a,22))+
          ((a&b)^(a&c)^(b&c));
        v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
      }
      h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;
      h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    void update(const uint8_t* data, size_t n) {
      bytes += n;
      while (n) {
        const size_t take = n < 64-filled ? n : 64-filled;
        memcpy(block+filled,data,take);
        filled += take;data += take;n -= take;
        if (filled==64) { compress();filled=0; }
      }
    }
    void finish(char hexOut[65]) {
      const uint64_t bits=bytes*8;
      const uint8_t pad=0x80, zero=0;
      update(&pad,1);
      while (filled!=56) update(&zero,1);
      uint8_t length[8];
      for (unsigned i=0;i<8;++i) length[7-i]=uint8_t(bits>>(8*i));
      update(length,8);
      static const char digits[]="0123456789abcdef";
      for (unsigned i=0;i<8;++i)
        for (unsigned j=0;j<4;++j) {
          const uint8_t value=uint8_t(h[i]>>(24-8*j));
          hexOut[i*8+j*2]=digits[value>>4];
          hexOut[i*8+j*2+1]=digits[value&15];
        }
      hexOut[64]=0;
    }
  };

  bool loadIndex(NotesFile idx) {
    Cursor cursor(idx);
    char line[128];
    size_t len=0, offset=0;
    if (!nextLine(cursor,line,sizeof(line),len,offset)) return false;
    char* fields[5] = {line};
    size_t n=0;
    for (size_t i=0;i<len;++i)
      if (line[i]=='\t') {
        if (n==4) return false;
        line[i]=0;fields[++n]=line+i+1;
      }
    uint32_t rows=0;
    if (n!=4 || strcmp(fields[0],"#PJ-NOTES-IDX") ||
        strcmp(fields[1],"1") || strcmp(fields[2],"fnv1a32") ||
        strlen(fields[3])!=64 || !uintField(fields[4],rows,10) ||
        rows!=count_) return false;
    Sha256 sha;
    uint8_t buffer[128];
    for (size_t pos=0;pos<tsv_.size;) {
      size_t take=tsv_.size-pos;
      if (take>sizeof(buffer)) take=sizeof(buffer);
      if (!tsv_.read(tsv_.context,pos,buffer,take)) return false;
      sha.update(buffer,take);
      pos+=take;
    }
    char digest[65];
    sha.finish(digest);
    if (strcmp(fields[3],digest)!=0) return false;
    uint32_t lastHash=0;
    char lastKey[49]={};
    for (size_t i=0;i<count_;++i) {
      if (!nextLine(cursor,line,sizeof(line),len,offset)) return false;
      char* first=strchr(line,'\t');
      if (!first || first-line!=8) return false;
      *first++=0;
      char* second=strchr(first,'\t');
      if (!second) return false;
      *second++=0;
      if (!validKey(second)) return false;
      uint32_t hash=0, seek=0;
      if (!uintField(line,hash,16) || !uintField(first,seek,10) ||
          hash!=fnv(second) || seek>=tsv_.size ||
          (i && (hash<lastHash || (hash==lastHash && strcmp(second,lastKey)<=0))))
        return false;
      JazzNote record;
      if (seek) {
        uint8_t prior;
        if (!tsv_.read(tsv_.context,seek-1,&prior,1) || prior!='\n')
          return false;
      }
      if (!readRecord(seek,record) || strcmp(record.key,second)) return false;
      for (size_t prev=0;prev<i;++prev)
        if (index_[prev].offset==seek) return false;
      index_[i].hash=hash;
      index_[i].offset=seek;
      memcpy(index_[i].key,second,strlen(second)+1);
      lastHash=hash;
      memcpy(lastKey,second,strlen(second)+1);
    }
    uint8_t trailing;
    return cursor.ok() && !cursor.byte(trailing) && cursor.ok() &&
           cursor.position()==idx.size;
  }
};
} // namespace pocketjazz

#endif
