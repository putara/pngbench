#ifndef PICOPNG_H
#define PICOPNG_H

#include <stdlib.h>

template <typename T>
class buffer
{
private:
  size_t count;
  T* buf;

  buffer(const buffer<T>&);
  buffer<T>& operator =(const buffer<T>&);

  void error()
  {
    ::abort();
  }

  void reserve(size_t newSize)
  {
    if (this->count >= newSize) {
      this->count = newSize;
      return;
    }
    if (SIZE_MAX / sizeof(T) >= newSize) {
      T* newBuf = static_cast<T*>(::realloc(this->buf, sizeof(T) * newSize));
      if (newSize == 0) {
        this->count = 0;
        this->buf = 0;
        return;
      }
      if (newBuf) {
        this->count = newSize;
        this->buf = newBuf;
        return;
      }
    }
    error();
  }

  static void copy(T* dst, const T* src, size_t count)
  {
    while (count--) {
      *dst++ = *src++;
    }
  }

public:
  buffer()
    : count()
    , buf()
  {
  }
  explicit buffer(size_t count, const T& val = T())
    : count()
    , buf()
  {
    resize(count, val);
  }
  ~buffer()
  {
    clear();
  }
  bool empty() const
  {
    return this->count == 0;
  }
  size_t size() const
  {
    return this->count;
  }
  void clear()
  {
    ::free(this->buf);
    this->buf = NULL;
    this->count = 0;
  }
  void resize(size_t newSize, const T& val = T())
  {
    size_t cur = this->count;
    reserve(newSize);
    T* p = this->buf;
    for (size_t i = cur; i < newSize; i++) {
      *p++ = val;
    }
  }
  void copy(const T* src, size_t count)
  {
    reserve(count);
    copy(this->buf, src, count);
  }
  void swap(buffer<T>& src)
  {
    T* p = src.buf;
    src.buf = this->buf;
    this->buf = p;
    size_t c = src.count;
    src.count = this->count;
    this->count = c;
  }
  void append(const T* src, size_t count)
  {
    size_t cur = this->count;
    if (SIZE_MAX - cur >= count) {
      reserve(cur + count);
      copy(this->buf + cur, src, count);
      return;
    }
    error();
  }
  T* detach()
  {
    T* p = this->buf;
    this->buf = NULL;
    this->count = 0;
    return p;
  }
  T& operator[](ptrdiff_t i)
  {
    return this->buf[i];
  }
  const T& operator[](ptrdiff_t i) const
  {
    return this->buf[i];
  }
};

int pico_decodePNG(buffer<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, size_t in_size, bool convert_to_rgba32 = true);

#endif
