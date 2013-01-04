#ifndef _GLIM_LDB_HPP_INCLUDED
#define _GLIM_LDB_HPP_INCLUDED

/**
 * Leveldb (http://code.google.com/p/leveldb/) wrapper.
 * @code
Copyright 2012 Kozarezov Artem Aleksandrovich

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
 * @endcode
 * @file
 */

#include <string>
#include <unordered_map>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/noncopyable.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/range/iterator_range.hpp>

#include <arpa/inet.h> // htonl, ntohl
#include <sys/stat.h> // mkdir
#include <sys/types.h> // mkdir
#include <string.h> // strerror
#include <errno.h>

#include "gstring.hpp"
#include "exception.hpp"

namespace glim {

G_DEFINE_EXCEPTION (LdbEx);

template <typename T> inline void ldbSerialize (gstring& bytes, const T& data) {
  gstring_stream stream (bytes);
  boost::archive::binary_oarchive oa (stream, boost::archive::no_header);
  oa << data;
}
template <typename V> inline void ldbDeserialize (const gstring& bytes, V& data) {
  gstring_stream stream (const_cast<gstring&> (bytes));
  boost::archive::binary_iarchive ia (stream, boost::archive::no_header);
  ia >> data;
}

/** uint32_t keys are stored big-endian (network byte order) in order to be compatible with lexicographic ordering. */
template <> inline void ldbSerialize<uint32_t> (gstring& bytes, const uint32_t& ui) {
  uint32_t nui = htonl (ui); bytes.append ((const char*) &nui, sizeof (uint32_t));}
/** Deserialize uint32_t from big-endian (network byte order). */
template <> inline void ldbDeserialize<uint32_t> (const gstring& bytes, uint32_t& ui) {
  if (bytes.size() != sizeof (uint32_t)) GNTHROW (LdbEx, "Not uint32_t, wrong number of bytes");
  uint32_t nui = * (uint32_t*) bytes.data(); ui = ntohl (nui);}

/** If the data is `gstring` then use the data's buffer directly, no copy. */
template <> inline void ldbSerialize<gstring> (gstring& bytes, const gstring& data) {
  bytes = gstring (0, (void*) data.data(), false, data.length());}
/** Deserializing into `gstring` copies the bytes into it, reusing its buffer. */
template <> inline void ldbDeserialize<gstring> (const gstring& bytes, gstring& data) {
  data.clear() << bytes;}

/** If the data is `std::string` then use the data's buffer directly, no copy. */
template <> inline void ldbSerialize<std::string> (gstring& bytes, const std::string& data) {
  bytes = gstring (0, (void*) data.data(), false, data.length());}
/** Deserializing into `std::string` copies the bytes into it, reusing its buffer. */
template <> inline void ldbDeserialize<std::string> (const gstring& bytes, std::string& data) {
  data.clear(); data.append (bytes.data(), bytes.size());}

/**
 * Header-only Leveldb wrapper.\n
 * Uses Boost Serialization to pack keys and values (glim::gstring can be used for raw bytes).\n
 * Allows semi-automatic indexing with triggers.
 */
struct Ldb {
  std::shared_ptr<leveldb::DB> _db;

  struct IteratorEntry {
    leveldb::Iterator* _lit;
    /** Zero-copy view of the current key bytes. Should *not* be used after the Iterator is changed or destroyed. */
    const gstring keyView() const {
      const leveldb::Slice& key = _lit->key();
      return gstring (0, (void*) key.data(), false, key.size(), true);} // Zero copy.
    /** Zero-copy view of the current value bytes. Should *not* be used after the Iterator is changed or destroyed. */
    const gstring valueView() const {
      const leveldb::Slice& val = _lit->value();
      return gstring (0, (void*) val.data(), false, val.size(), true);} // Zero copy.
    /** Deserialize into `key`. */
    template <typename T> void getKey (T& key) const {ldbDeserialize (keyView(), key);}
    /** Deserialize the key into a temporary and return it. */
    template <typename T> T getKey() const {T key; getKey (key); return key;}
    /** Deserialize into `value`. */
    template <typename T> void getValue (T& value) const {ldbDeserialize (valueView(), value);}
    /** Deserialize the value into a temporary and return it. */
    template <typename T> T getValue() const {T value; getValue (value); return value;}
  };
  /** Wraps Leveldb iterator. */
  struct Iterator: public boost::iterator_facade<Iterator, IteratorEntry, boost::bidirectional_traversal_tag> {
    std::shared_ptr<leveldb::Iterator> _lit; // Iterator might be copied around, therefore we keep the real iterator in shared_ptr.
    IteratorEntry _entry;

    Iterator (const Iterator&) = default;
    Iterator (Iterator&&) = default;
    /** Iterate from the beginning or the end of the database.
     * @param position can be MDB_FIRST or MDB_LAST */
    Iterator (Ldb* ldb, leveldb::ReadOptions options = leveldb::ReadOptions()): _lit (ldb->_db->NewIterator (options)) {
      _lit->SeekToFirst();
    }

    struct EndIteratorFlag {};
    Iterator (Ldb* ldb, EndIteratorFlag, leveldb::ReadOptions options = leveldb::ReadOptions()): _lit (ldb->_db->NewIterator (options)) {}
    /** True if the iterator isn't pointing anywhere. */
    bool end() const {return !_lit->Valid();}

    Iterator (const std::shared_ptr<leveldb::Iterator>& lit): _lit (lit) {}
    Iterator (std::shared_ptr<leveldb::Iterator>&& lit): _lit (std::move (lit)) {}

    bool equal (const Iterator& other) const {
      bool weAreValid = _lit->Valid(), theyAreValid = other->_lit->Valid();
      if (!weAreValid) return !theyAreValid;
      if (!theyAreValid) return false;
      auto&& ourKey = _lit->key(), theirKey = other->_lit->key();
      if (ourKey.size() != theirKey.size()) return false;
      return memcmp (ourKey.data(), theirKey.data(), ourKey.size()) == 0;
    }
    IteratorEntry& dereference() const {
      IteratorEntry& entry = const_cast<IteratorEntry&> (_entry);
      entry._lit = _lit.get();
      // NB: Boost iterator_facade expects the `dereference` to be a `const` method.
      //     I guess Iterator is not modified, so the `dereference` is `const`, even though the Entry can be modified.
      return entry;
    }
    void increment() {
      if (_lit->Valid()) _lit->Next();
      else _lit->SeekToFirst();
    }
    void decrement() {
      if (_lit->Valid()) _lit->Prev();
      else _lit->SeekToLast();
    }
  };
  Iterator begin() {return Iterator (this);}
  const Iterator end() {return Iterator (this, Iterator::EndIteratorFlag());}

  struct Trigger {
    virtual gstring triggerName() const {return C2GSTRING ("defaultTriggerName");};
    virtual void put (Ldb& ldb, void* key, gstring& kbytes, void* value, gstring& vbytes, leveldb::WriteBatch& batch) = 0;
    virtual void del (Ldb& ldb, void* key, gstring& kbytes, leveldb::WriteBatch& batch) = 0;
  };
  std::unordered_map<gstring, std::shared_ptr<Trigger>> _triggers;

  /** Register the trigger (by its `triggerName`). */
  void putTrigger (std::shared_ptr<Trigger> trigger) {
    _triggers[trigger->triggerName()] = trigger;
  }

 public:

  Ldb() {}

  /** Opens Leveldb database. */
  Ldb (const char* path, leveldb::Options* options = nullptr, mode_t mode = 0770) {
    int rc = ::mkdir (path, mode);
    if (rc && errno != EEXIST) GNTHROW (LdbEx, std::string ("Can't create ") + path + ": " + ::strerror (errno));
    leveldb::DB* db;
    leveldb::Status status;
    if (options) {
      status = leveldb::DB::Open (*options, path, &db);
    } else {
      leveldb::Options localOptions;
      localOptions.create_if_missing = true;
      status = leveldb::DB::Open (localOptions, path, &db);
    }
    if (!status.ok()) GNTHROW (LdbEx, std::string ("Ldb: Can't open ") + path + ": " + status.ToString());
    _db.reset (db);
  }

  /** Wraps an existing Leveldb handler. */
  Ldb (std::shared_ptr<leveldb::DB> db): _db (db) {}

  template <typename K, typename V> void put (const K& key, const V& value, leveldb::WriteBatch& batch) {
    char kbuf[64]; // Allow up to 64 bytes to be serialized without heap allocations.
    gstring kbytes (sizeof (kbuf), kbuf, false, 0);
    ldbSerialize (kbytes, key);

    char vbuf[64]; // Allow up to 64 bytes to be serialized without heap allocations.
    gstring vbytes (sizeof (vbuf), vbuf, false, 0);
    ldbSerialize (vbytes, value);

    for (auto& trigger: _triggers) trigger.second->put (*this, (void*) &key, kbytes, (void*) &value, vbytes, batch);

    batch.Put (leveldb::Slice (kbytes.data(), kbytes.size()), leveldb::Slice (vbytes.data(), vbytes.size()));
  }
  template <typename K, typename V> void put (const K& key, const V& value) {
    leveldb::WriteBatch batch;
    put (key, value, batch);
    leveldb::Status status (_db->Write (leveldb::WriteOptions(), &batch));
    if (!status.ok()) GNTHROW (LdbEx, "Ldb: add: " + status.ToString());
  }

  /** Returns `true` if the key exists. Throws on error. */
  template <typename K> bool have (const K& key, leveldb::ReadOptions options = leveldb::ReadOptions()) {
    char kbuf[64]; // Allow up to 64 bytes to be serialized without heap allocations.
    gstring kbytes (sizeof (kbuf), kbuf, false, 0);
    ldbSerialize (kbytes, key);
    leveldb::Slice keySlice (kbytes.data(), kbytes.size());

    leveldb::Status status (_db->Get (options, keySlice, nullptr));
    if (status.ok()) return true;
    if (status.IsNotFound()) return false;
    throw std::runtime_error ("Ldb.have: " + status.ToString());
  }

  template <typename K, typename V> bool get (const K& key, V& value, leveldb::ReadOptions options = leveldb::ReadOptions()) {
    char kbuf[64]; // Allow up to 64 bytes to be serialized without heap allocations.
    gstring kbytes (sizeof (kbuf), kbuf, false, 0);
    ldbSerialize (kbytes, key);
    leveldb::Slice keySlice (kbytes.data(), kbytes.size());

//    leveldb::Status status (_db->Get (options, keySlice, &vbytes));
//    if (status.IsNotFound()) return false;
//    if (!status.ok()) GNTHROW (LdbEx, "Ldb: first: " + status.ToString());
//    ldbDeserialize (vbytes, value);

    // Using an iterator to avoid the std::string copy of value in `Get` (I don't know if it is actually faster).
    std::unique_ptr<leveldb::Iterator> it (_db->NewIterator (options));
    it->Seek (keySlice); if (it->Valid()) {
      auto&& itKey = it->key();
      if (itKey.size() == keySlice.size() && memcmp (itKey.data(), keySlice.data(), itKey.size()) == 0) {
        auto&& vbytes = it->value();
        ldbDeserialize (gstring (0, (void*) vbytes.data(), false, vbytes.size()), value);
        return true;
      }
    }
    return false;
  }

  template <typename K> void del (const K& key, leveldb::WriteBatch& batch) {
    char kbuf[64]; // Allow up to 64 bytes to be serialized without heap allocations.
    gstring kbytes (sizeof (kbuf), kbuf, false, 0);
    ldbSerialize (kbytes, key);
    if (kbytes.empty()) GNTHROW (LdbEx, "del: key is empty");

    for (auto& trigger: _triggers) trigger.second->del (*this, (void*) &key, kbytes, batch);

    batch.Delete (leveldb::Slice (kbytes.data(), kbytes.size()));
  }
  template <typename K> void del (const K& key) {
    leveldb::WriteBatch batch;
    del (key, batch);
    leveldb::Status status (_db->Write (leveldb::WriteOptions(), &batch));
    if (!status.ok()) GNTHROW (LdbEx, "Ldb: del: " + status.ToString());
  }

  virtual ~Ldb() {
    _triggers.clear(); // Destroy triggers before closing the database.
  }
};

} // namespace glim

#endif // _GLIM_LDB_HPP_INCLUDED