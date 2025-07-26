#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <utility>

/**
 * @brief Computes a 64-bit hash for a given string using a simplified Wyhash-style hash.
 *
 * @param s Input string
 * @return 64-bit hash value
 */
static uint64_t wyhash_str(const std::string &s)
{
  uint64_t hash = 0xa0761d6478bd642fULL ^ s.size();
  for (char c : s)
    hash = (hash ^ uint8_t(c)) * 0xe7037ed1a0b428dbULL;
  hash ^= (hash >> 33);
  return hash;
}

/**
 * @brief A lightweight open-addressed hash map specialized for string-to-string mapping.
 *
 * Uses linear probing, fingerprinting for fast comparisons, and supports O(1) access.
 * Does not preserve insertion order. Not thread-safe.
 */
class Leaf_map
{
  struct bucket
  {
    uint8_t ctrl;      ///< Control byte: high bit = empty/deleted, low 7 bits = hash fingerprint
    std::string key;   ///< Key string
    std::string value; ///< Value string
    bucket() : ctrl(0x80) {} ///< Default: mark as empty
  };

  std::vector<bucket> _store; ///< Hash table buckets
  size_t _mask;               ///< Mask for mod-indexing
  size_t _size;               ///< Number of live entries
  float _max_load;            ///< Max load factor before resizing

  static constexpr uint8_t h2(uint64_t h) { return static_cast<uint8_t>(h >> 57); }

public:
  /**
   * @brief Construct a fast_strmap with optional initial capacity.
   *
   * @param initial Minimum capacity (rounded to next power of 2)
   */
  Leaf_map(size_t initial = 16)
      : _store(next_pow2(initial)), _mask(_store.size() - 1), _size(0), _max_load(0.65f) {}

  /**
   * @brief Insert or update a key-value pair.
   *
   * @param k Key
   * @param v Value
   * @return Pointer to stored value string
   */
  std::string *put(const std::string &k, const std::string &v)
  {
    grow_if_needed();
    uint64_t h = wyhash_str(k);
    uint8_t fp = h2(h);
    size_t idx = h & _mask, dist = 0;

    for (;;)
    {
      bucket &b = _store[idx];
      if (is_empty(b))
      {
        // Empty slot found — insert new pair
        b.ctrl = fp;
        b.key = k;
        b.value = v;
        ++_size;
        return &b.value;
      }
      if (b.ctrl == fp && b.key == k)
      {
        // Key match — overwrite
        b.value = v;
        return &b.value;
      }
      // Continue probing
      ++dist;
      ++idx;
      idx &= _mask;
    }
  }

  /**
   * @brief Lookup key. Returns pointer to value if found, or nullptr.
   *
   * @param k Key to search
   * @return Pointer to stored value, or nullptr
   */
  std::string *get(const std::string &k)
  {
    uint64_t h = wyhash_str(k);
    uint8_t fp = h2(h);
    size_t idx = h & _mask, dist = 0;

    for (;;)
    {
      bucket &b = _store[idx];
      if (is_empty(b))
        return nullptr;

      // Fast fingerprint + first-char shortcut, then full key compare
      if (b.ctrl == fp && b.key[0] == k[0] && b.key == k)
        return &b.value;

      ++dist;
      ++idx;
      idx &= _mask;
    }
  }

  /**
   * @brief Safe lookup with optional reference wrapper
   *
   * @param k Key to search
   * @return std::optional<std::reference_wrapper<std::string>> 
   *         Empty if key not found, wrapped reference if found
   */
  std::optional<std::reference_wrapper<std::string>> get_opt(const std::string& k) {
    if (auto p = get(k)) {
      return std::ref(*p);
    }
    return std::nullopt;
  }

  /**
   * @brief Const version of safe lookup
   */
  std::optional<std::reference_wrapper<const std::string>> get_opt(const std::string& k) const {
    if (auto p = get(k)) {
      return std::cref(*p);
    }
    return std::nullopt;
  }

  /// Const overload of get()
  std::string *get(const std::string &k) const { return const_cast<Leaf_map *>(this)->get(k); }

  /**
   * @brief Erase entry by key. Returns true if key existed.
   */
  bool erase(const std::string &k)
  {
    uint64_t h = wyhash_str(k);
    uint8_t fp = h2(h);
    size_t idx = h & _mask;

    for (;;)
    {
      bucket &b = _store[idx];
      if (is_empty(b))
        return false;
      if (b.ctrl == fp && b.key == k)
      {
        b.ctrl = 0x80; // Mark as empty
        backward_shift(idx);
        --_size;
        return true;
      }
      ++idx;
      idx &= _mask;
    }
  }

  /**
   * @brief Returns number of entries in map.
   */
  size_t size() const { return _size; }

  /**
   * @brief Returns true if map is empty.
   */
  bool empty() const { return _size == 0; }

  /**
   * @brief Reserve at least `n` elements of capacity.
   */
  void reserve(size_t n)
  {
    if (n > _store.size() * _max_load)
      rehash(next_pow2(n / _max_load + 1));
  }

  /**
   * @brief Iterator for key-value pairs in the map.
   * Provides `{const std::string&, std::string&}` on dereference.
   */
  struct iterator
  {
    const std::vector<bucket> *_store;
    size_t _idx, _end;

    iterator(const std::vector<bucket> *s, size_t i)
        : _store(s), _idx(i), _end(s->size()) { skip(); }

    void skip()
    {
      while (_idx < _end && is_empty((*_store)[_idx])) ++_idx;
    }

    iterator &operator++()
    {
      ++_idx;
      skip();
      return *this;
    }

    bool operator!=(const iterator &o) const { return _idx != o._idx; }

    auto operator*() const -> std::pair<const std::string &, std::string &>
    {
      auto &b = const_cast<bucket &>((*_store)[_idx]);
      return {b.key, b.value};
    }
  };

  /// Returns iterator to beginning
  iterator begin() const { return iterator(&_store, 0); }

  /// Returns iterator to end
  iterator end() const { return iterator(&_store, _store.size()); }

  /**
   * @brief Lookup with optional fallback (UX wrapper).
   *
   * @param key Key to lookup
   * @param fallback Value to return if key is not found
   * @return Reference to found value or fallback
   */
  const std::string &get_or(const std::string &key, const std::string &fallback) const
  {
    auto *p = get(key);
    return p ? *p : fallback;
  }

  /**
   * @brief Check if key exists.
   */
  bool contains(const std::string &key) const
  {
    return get(key) != nullptr;
  }

private:
  static size_t next_pow2(size_t v)
  {
    size_t n = 1;
    while (n < v) n <<= 1;
    return n;
  }

  static bool is_empty(const bucket &b) { return b.ctrl & 0x80; }

  void grow_if_needed()
  {
    if ((_size + 1) > _store.size() * _max_load)
      rehash(_store.size() * 2);
  }

  void rehash(size_t newcap)
  {
    std::vector<bucket> newstore(newcap);
    size_t newmask = newcap - 1;

    for (auto &b : _store)
    {
      if (!is_empty(b))
      {
        uint64_t h = wyhash_str(b.key);
        size_t idx = h & newmask, dist = 0;
        uint8_t fp = h2(h);

        for (;; ++dist, ++idx, idx &= newmask)
        {
          bucket &dst = newstore[idx];
          if (is_empty(dst))
          {
            dst.ctrl = fp;
            dst.key = std::move(b.key);
            dst.value = std::move(b.value);
            break;
          }
        }
      }
    }

    _store.swap(newstore);
    _mask = newmask;
  }

  void backward_shift(size_t hole)
  {
    size_t idx = (hole + 1) & _mask;

    for (;;)
    {
      auto &b = _store[idx];
      if (is_empty(b))
        break;

      uint64_t ideal = wyhash_str(b.key) & _mask;
      if (distance(ideal, idx) == 0)
        break;

      _store[hole] = std::move(b);
      b.ctrl = 0x80;
      hole = idx;
      idx = (hole + 1) & _mask;
    }
  }

  static size_t distance(size_t a, size_t b)
  {
    return (b + (1ull << 63) - a) & ((1ull << 63) - 1);
  }
};
