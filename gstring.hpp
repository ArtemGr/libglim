#ifndef _GSTRING_INCLUDED
#define _GSTRING_INCLUDED

#include <assert.h>
#include <stdlib.h> // malloc, realloc, free
#include <stdint.h>
#include <string.h> // memcpy
#include <stdexcept>
#include <iostream>

// Make a read-only gstring from a C string: `const gstring foo = C2GSTRING("foo")`.
#define C2GSTRING(cstr) (static_cast<const ::glim::gstring> (::glim::gstring (0, (void*) cstr, false, true, sizeof (cstr) - 1)))

namespace glim {

/**
 * Based on: C++ version 0.4 char* style "itoa": Written by Lukás Chmela, http://www.strudel.org.uk/itoa/ (GPLv3).
 * Returns a pointer to the end of the string.
 * NB about `inline`: http://stackoverflow.com/a/1759575/257568
 */
inline char* itoa (char* ptr, int64_t value, const int base = 10) {
  // check that the base is valid
  if (base < 2 || base > 36) {*ptr = '\0'; return ptr;}

  char *ptr1 = ptr;
  int64_t tmp_value;

  do {
    tmp_value = value;
    value /= base;
    *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
  } while (value);

  // Apply negative sign
  if (tmp_value < 0) *ptr++ = '-';
  char* end = ptr;
  *ptr-- = '\0';
  char tmp_char;
  while (ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr--= *ptr1;
    *ptr1++ = tmp_char;
  }
  return end;
}

class gstring_stream;

class gstring {
  enum Flags {
    FREE_FLAG = 0x80000000, // 1st bit; `_buf` needs `free`ing
    FREE_OFFSET = 31,
    RO_FLAG = 0x40000000, // 2nd bit; `_buf` is read-only
    RO_OFFSET = 30,
    CAPACITY_MASK = 0x3F000000, // 3..8 bits; `_buf` size is 2^this
    CAPACITY_OFFSET = 24,
    LENGTH_MASK = 0x00FFFFFF, // 9th bit; allocated capacity
  };
  uint32_t _meta;
public:
  void* _buf;
public:
  gstring(): _meta (0), _buf (NULL) {}
  /**
   * Reuse `buf` of size `size`.
   * To fully use `buf` the `size` should be the power of two.
   * @param bufSize The size of the memory allocated to `buf`.
   * @param buf The memory region to be reused.
   * @param free Whether the `buf` should be `free`d on resize or gstring destruction.
   * @param readOnly Whether the `buf` is read-only and should not be written to.
   * @param length String length inside the `buf`.
   */
  explicit gstring (uint32_t bufSize, void* buf, bool free, bool readOnly, uint32_t length) {
    uint32_t power = 0; while (((uint32_t) 1 << (power + 1)) <= bufSize) ++power;
    _meta = ((uint32_t) free << FREE_OFFSET) |
            ((uint32_t) readOnly << RO_OFFSET) |
            (power << CAPACITY_OFFSET) |
            (length & LENGTH_MASK);
    _buf = buf;
  }

  /** Copy the characters into `gstring`. */
  gstring (const char* chars, size_t length) {
    if (length != 0) {
      _buf = ::malloc (length);
      ::memcpy (_buf, chars, length);
    }
    _meta = (uint32_t) FREE_FLAG |
            (length & LENGTH_MASK);
  }

  gstring (const gstring& gstr) {
    uint32_t glen = gstr.length();
    if (glen != 0) {
      _buf = ::malloc (glen);
      ::memcpy (_buf, gstr._buf, glen);
    }
    _meta = (uint32_t) FREE_FLAG |
            (glen & LENGTH_MASK);
  }
  gstring (gstring&& gstr): _meta (gstr._meta), _buf (gstr._buf) {
    gstr._meta = 0; gstr._buf = NULL;
  }
  gstring& operator = (const gstring& gstr) {
    // cf. http://stackoverflow.com/questions/9322174/move-assignment-operator-and-if-this-rhs
    if (this != &gstr) {
      uint32_t glen = gstr.length();
      uint32_t power = 0;
      if (glen <= capacity() && !isReadOnly()) {
        // We reuse existing buffer. Keep capacity info.
        power = (_meta & CAPACITY_MASK) >> CAPACITY_OFFSET;
      } else {
        if (_buf != NULL && needsFreeing()) ::free (_buf);
        _buf = ::malloc (glen);
        if (_buf == NULL) throw std::runtime_error ("malloc failed");
      }
      ::memcpy (_buf, gstr._buf, glen);
      _meta = (uint32_t) FREE_FLAG |
              (power << CAPACITY_OFFSET) |
              (glen & LENGTH_MASK);
    }
    return *this;
  }
  gstring& operator = (gstring&& gstr) {
    assert (this != &gstr);
    if (_buf != NULL && needsFreeing()) free (_buf);
    _meta = gstr._meta; _buf = gstr._buf;
    gstr._meta = 0; gstr._buf = NULL;
    return *this;
  }

  bool needsFreeing() const {return _meta & FREE_FLAG;}
  bool isReadOnly() const {return _meta & RO_FLAG;}
  /** Current buffer capacity (memory allocated to the string). Returns 1 if no memory allocated. */
  uint32_t capacity() const {return 1 << ((_meta & CAPACITY_MASK) >> CAPACITY_OFFSET);}
  uint32_t length() const {return _meta & LENGTH_MASK;}
  size_t size() const {return _meta & LENGTH_MASK;}
  bool empty() const {return (_meta & LENGTH_MASK) == 0;}
  std::string str() const {return std::string ((const char*) _buf, size());}
  const char* c_str() {
    uint32_t len = length();
    if (len == 0) return "";
    uint32_t cap = capacity();
    const char* buf = (const char*) _buf;
    if ((isReadOnly() || len < cap) && buf[len] == 0) return buf;
    append (0);
    setLength (len);
    return (const char*) _buf;
  }
  bool equals (const char* cstr) const {
    const char* cstr_; uint32_t clen_;
    if (cstr != NULL) {cstr_ = cstr; clen_ = strlen (cstr);} else {cstr_ = ""; clen_ = 0;}
    const uint32_t len = length();
    if (len != clen_) return false;
    const char* gstr_ = _buf != NULL ? (const char*) _buf : "";
    return strncmp (gstr_, cstr_, len) == 0;
  }
  bool equals (const gstring& gs) const {
    uint32_t llen = length(), olen = gs.length();
    if (llen != olen) return false;
    return strncmp ((const char*) _buf, (const char*) gs._buf, llen) == 0;
  }

  char& operator[] (unsigned index) {return ((char*)_buf)[index];}
  const char& operator[] (unsigned index) const {return ((const char*)_buf)[index];}

  /// Access `_buf` as `char*`. `_buf` might be NULL.
  char* data() {return (char*)_buf;}

protected:
  friend class gstring_stream;
  void grow() {
    uint32_t len = length();
    uint32_t power = (_meta & CAPACITY_MASK) >> CAPACITY_OFFSET;
    do {++power;} while (((uint32_t) 1 << power) < len);
    _meta = (_meta & ~CAPACITY_MASK) | (power << CAPACITY_OFFSET);
    if (needsFreeing()) {
      _buf = ::realloc (_buf, capacity());
      if (_buf == NULL) throw std::runtime_error ("realloc failed");
    } else {
      const char* oldBuf = (const char*) _buf;
      _buf = ::malloc (capacity());
      if (_buf == NULL) throw std::runtime_error ("malloc failed");
      if (oldBuf != NULL) ::memcpy (_buf, oldBuf, length());
      _meta |= FREE_FLAG;
    }
  }
  void setLength (uint32_t len) {
    _meta = (_meta & ~LENGTH_MASK) | (len & LENGTH_MASK);
  }
  void append64 (int64_t iv, int bytes = 24) {
    if (isReadOnly()) throw std::runtime_error ("gstring is read-only");
    uint32_t pos = length();
    while (capacity() < pos + bytes) grow();
    setLength (itoa ((char*) _buf + pos, iv, 10) - (char*) _buf);
  }
public:
  void append (char ch) {
    if (isReadOnly()) throw std::runtime_error ("gstring is read-only");
    uint32_t pos = length();
    if (pos >= capacity() || _buf == NULL) grow();
    ((char*)_buf)[pos] = ch;
    setLength (++pos);
  }
  void append (const char* cstr, uint32_t clen) {
    uint32_t len = length();
    uint32_t need = len + clen;
    while (need > capacity()) grow();
    ::memcpy ((char*) _buf + len, cstr, clen);
    setLength (need);
  }
  gstring& operator << (const std::string& str) {append (str.data(), str.length()); return *this;}
  gstring& operator << (const char* cstr) {append (cstr, ::strlen (cstr)); return *this;}
  gstring& operator << (char ch) {append (ch); return *this;}
  gstring& operator << (int iv) {append64 (iv, sizeof (int) * 3); return *this;}
  gstring& operator << (long iv) {append64 (iv, sizeof (long) * 3); return *this;}
  gstring& operator << (long long iv) {append64 (iv, sizeof (long long) * 3); return *this;}

  /// Append the characters to this `gstring` wrapping them in the netstring format.
  gstring& appendNetstring (const char* cstr, uint32_t clen) {
    *this << (int) clen; append (':'); append (cstr, clen); append (','); return *this;}
  /// Append the `str` to this `gstring` wrapping it in the netstring format.
  gstring& appendNetstring (const std::string& str) {return appendNetstring (str.data(), str.size());}

  std::ostream& writeAsNetstring (std::ostream& stream) const;

  /// Parse netstring at `pos` and return a `gstring` *pointing* at the parsed netstring.\n
  /// No heap space allocated.\n
  /// Throws std::runtime_error if netstring parsing fails.\n
  /// If parsing was successfull, then `after` is set to point after the parsed netstring.
  gstring netstringAt (uint32_t pos, uint32_t* after = NULL) const {
    const uint32_t len = length(); char* buf = (char*) _buf;
    if (buf == NULL) throw std::runtime_error ("gstring: netstringAt: NULL");
    uint32_t next = pos;
    while (next < len && buf[next] >= '0' && buf[next] <= '9') ++next;
    if (next >= len || buf[next] != ':' || next - pos > 10) throw std::runtime_error ("gstring: netstringAt: no header");
    char* endptr = 0;
    long nlen = ::strtol (buf + pos, &endptr, 10);
    if (endptr != buf + next) throw std::runtime_error ("gstring: netstringAt: unexpected header end");
    pos = next + 1; next = pos + nlen;
    if (next >= len || buf[next] != ',') throw std::runtime_error ("gstring: netstringAt: no body");
    if (after) *after = next + 1;
    return gstring (0, buf + pos, false, false, next - pos);
  }

  /// Wrapper around strtol, not entirely safe (make sure the string is terminated with a non-digit).
  long intAt (uint32_t pos, uint32_t* after = NULL, int base = 10) const {
    const uint32_t len = length(); char* buf = (char*) _buf;
    if (pos >= len || buf == NULL) throw std::runtime_error ("gstring: intAt: pos >= len");
    char* endptr = 0;
    long lv = ::strtol (buf + pos, &endptr, base);
    uint32_t next = endptr - buf;
    if (next >= len) throw std::runtime_error ("gstring: intAt: endptr >= len");
    if (after) *after = next;
    return lv;
  }

  /// Get a single netstring from the `stream` and append it to the end of `gstring`.
  /// Throws an exception if the input is not a well-formed netstring.
  gstring& readNetstring (std::istream& stream) {
    int32_t nlen; stream >> nlen;
    if (!stream.good() || nlen < 0) throw std::runtime_error ("!netstring");
    int ch = stream.get();
    if (!stream.good() || ch != ':') throw std::runtime_error ("!netstring");
    uint32_t glen = length();
    while (capacity() < glen + nlen) grow();
    stream.read ((char*) _buf + glen, nlen);
    if (!stream.good()) throw std::runtime_error ("!netstring");
    ch = stream.get();
    if (ch != ',') throw std::runtime_error ("!netstring");
    setLength (glen + nlen);
    return *this;
  }

  /// Set length to 0. `_buf` not changed.
  gstring& clear() {setLength (0); return *this;}

  ~gstring() {
    if (_buf != NULL && needsFreeing()) {::free (_buf); _buf = NULL;}
  }
};

inline bool operator == (const char* cstr, const gstring& gstr) {return gstr.equals (cstr);}
inline bool operator == (const gstring& gstr, const char* cstr) {return gstr.equals (cstr);}
inline bool operator != (const gstring& gs1, const gstring& gs2) {return !gs1.equals (gs2);}
inline bool operator != (const char* cstr, const gstring& gstr) {return !gstr.equals (cstr);}
inline bool operator != (const gstring& gstr, const char* cstr) {return !gstr.equals (cstr);}

inline std::ostream& operator << (std::ostream& os, const gstring& gstr) {
  if (gstr._buf != NULL) os.write ((const char*) gstr._buf, gstr.length());
  return os;
}

/// Encode this `gstring` into `stream` as a netstring.
inline std::ostream& gstring::writeAsNetstring (std::ostream& stream) const {
  stream << length() << ':' << *this << ',';
  return stream;
}

// http://www.mr-edd.co.uk/blog/beginners_guide_streambuf
// http://www.dreamincode.net/code/snippet2499.htm
// http://spec.winprog.org/streams/
class gstring_stream: public std::basic_streambuf<char, std::char_traits<char> > {
  gstring& _gstr;
public:
  gstring_stream (gstring& gstr): _gstr (gstr) {
    char* buf = (char*) gstr._buf;
    if (buf != NULL) setg (buf, buf, buf + gstr.length());
  }
protected:
  virtual int_type overflow (int_type ch) {
    if (ch != traits_type::eof()) _gstr.append ((char) ch);
    return ch;
  }
  // no copying
  gstring_stream (const gstring_stream &);
  gstring_stream& operator = (const gstring_stream &);
};

} // namespace glim

#endif // _GSTRING_INCLUDED
