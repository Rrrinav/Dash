#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <optional>

class Leaf_map
{
  // === Constants for control byte markers ===
  static constexpr uint8_t EMPTY = 0x80;    // 0b10000000
  static constexpr uint8_t DELETED = 0xFE;  // 0b11111110
  static constexpr size_t MIN_CAPACITY = 16;
  static constexpr size_t GROUP_SIZE = 16;
  static constexpr float MAX_LOAD = 0.65f;
  static constexpr size_t INITIAL_ARENA_SIZE = 2048;

  // === Represents a key-value pair stored in the hash table ===
  struct Entry
  {
    uint64_t short_key;
    const char *key = nullptr;
    const char *value = nullptr;
    size_t key_len = 0;
    size_t value_len = 0;
    uint64_t hash = 0;  // Cached hash for faster comparisons
  };

  // === Arena allocator block for allocating key-value memory ===
  struct Arena_block
  {
    char *data;
    size_t size;
    size_t used = 0;

    Arena_block(size_t size) : size(size), data(new char[size]) {}

    // Move constructor
    Arena_block(Arena_block &&other) noexcept : data(other.data), size(other.size), used(other.used)
    {
      other.data = nullptr;
      other.size = 0;
      other.used = 0;
    }

    // Move assignment operator
    Arena_block &operator=(Arena_block &&other) noexcept
    {
      if (this != &other)
      {
        delete[] data;
        data = other.data;
        size = other.size;
        used = other.used;

        other.data = nullptr;
        other.size = 0;
        other.used = 0;
      }
      return *this;
    }

    // Disable copying
    Arena_block(const Arena_block &) = delete;
    Arena_block &operator=(const Arena_block &) = delete;

    ~Arena_block() { delete[] data; }
  };

  // === Internal state ===
  std::vector<uint8_t> control;           // Control bytes per slot
  std::vector<Entry> entries;             // Actual entries (key-value pairs)
  std::vector<Arena_block> arena_blocks;  // Arena-allocated key/value storage
  size_t capacity;
  size_t count;
  size_t mask;
  size_t arena_block_size = INITIAL_ARENA_SIZE;

  // === Hash function (FNV-1a 64-bit) ===
  static uint64_t hash_string(const char *data, size_t size) noexcept
  {
    constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325;
    constexpr uint64_t FNV_PRIME = 0x100000001b3;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < size; ++i)
    {
      hash ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i]));
      hash *= FNV_PRIME;
    }
    return hash;
  }

  // === Compute next power of two >= n ===
  static size_t next_power_of_two(size_t n)
  {
    if (n <= 1)
      return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
  }

  // === Allocate memory from current or new arena block ===
  char *arena_allocate(size_t size)
  {
    if (!arena_blocks.empty())
    {
      Arena_block &block = arena_blocks.back();
      if (block.used + size <= block.size)
      {
        char *ptr = block.data + block.used;
        block.used += size;
        return ptr;
      }
    }

    // Allocate new arena block
    size_t new_block_size = std::max(arena_block_size, size);
    arena_blocks.emplace_back(new_block_size);
    arena_block_size *= 2;  // Exponential growth

    Arena_block &new_block = arena_blocks.back();
    char *ptr = new_block.data;
    new_block.used = size;
    return ptr;
  }

  // === Resize and rehash all entries ===
  void rehash(size_t new_capacity)
  {
    new_capacity = next_power_of_two(new_capacity);
    std::vector<uint8_t> old_control = std::move(control);
    std::vector<Entry> old_entries = std::move(entries);
    std::vector<Arena_block> old_arena_blocks = std::move(arena_blocks);

    control.clear();
    control.resize(new_capacity, EMPTY);
    entries.clear();
    entries.resize(new_capacity);
    arena_blocks.clear();
    arena_block_size = INITIAL_ARENA_SIZE;

    capacity = new_capacity;
    mask = capacity - 1;
    count = 0;

    for (size_t i = 0; i < old_control.size(); i++)
    {
      if (old_control[i] != EMPTY && old_control[i] != DELETED)
      {
        const Entry &e = old_entries[i];
        put_impl(e.key, e.key_len, e.value, e.value_len, e.hash);
      }
    }
  }

  // === Core insertion logic used by both put() and rehash() ===
  void put_impl(const char *key, size_t key_len, const char *value, size_t value_len, uint64_t hash)
  {
    const uint8_t H2 = (hash >> 57) & 0x7F;
    size_t start_index = hash & mask;
    size_t insert_index = SIZE_MAX;
    bool found = false;

    uint64_t smol{};
    if (key_len < 8) std::memcpy(&smol, key, key_len);
    // Probe for matching or empty slot
    for (size_t i = 0; i < capacity; i++)
    {
      size_t index = (start_index + i) & mask;
      uint8_t &ctrl = control[index];

      if (ctrl == EMPTY)
      {
        if (insert_index == SIZE_MAX)
          insert_index = index;
        break;
      }
      else if (ctrl == DELETED)
      {
        if (insert_index == SIZE_MAX)
          insert_index = index;
      }
      else if (ctrl == H2)
      {
        Entry &e = entries[index];
        if (e.hash == hash && e.key[0] == key[0] && std::memcmp(e.key, key, key_len) == 0)
        {
          // Update existing entry
          char *new_data = arena_allocate(key_len + value_len + 2);
          std::memcpy(new_data, key, key_len);
          new_data[key_len] = '\0';
          std::memcpy(new_data + key_len + 1, value, value_len);
          new_data[key_len + 1 + value_len] = '\0';

          e.short_key = smol;
          e.key = new_data;
          e.value = new_data + key_len + 1;
          e.key_len = key_len;
          e.value_len = value_len;
          found = true;
          break;
        }
      }
    }

    if (!found && insert_index != SIZE_MAX)
    {
      // New entry: allocate key+value memory
      char *data = arena_allocate(key_len + value_len + 2);
      std::memcpy(data, key, key_len);
      data[key_len] = '\0';
      std::memcpy(data + key_len + 1, value, value_len);
      data[key_len + 1 + value_len] = '\0';

      Entry &e = entries[insert_index];
      e.short_key = smol;
      e.key = data;
      e.value = data + key_len + 1;
      e.key_len = key_len;
      e.value_len = value_len;
      e.hash = hash;

      control[insert_index] = H2;
      count++;
    }
  }

public:
  // === Constructor with optional initial capacity ===
  Leaf_map(size_t initial_capacity = MIN_CAPACITY) : capacity(next_power_of_two(initial_capacity)), count(0), mask(capacity - 1)
  {
    control.resize(capacity, EMPTY);
    entries.resize(capacity);
  }

  // === Public insertion API ===
  void put(const std::string &key, const std::string &value)
  {
    if (count + 1 > capacity * MAX_LOAD)
      rehash(capacity * 2);

    const uint64_t hash = hash_string(key.data(), key.size());
    put_impl(key.data(), key.size(), value.data(), value.size(), hash);
  }

  // === Lookup an entry by key ===
  std::optional<std::string> get(const std::string &key) const
  {
    if (count == 0)
      return std::nullopt;

    const uint64_t hash = hash_string(key.data(), key.size());
    const uint8_t H2 = (hash >> 57) & 0x7F;
    size_t start_index = hash & mask;

    for (size_t i = 0; i < capacity; i++)
    {
      size_t index = (start_index + i) & mask;
      const uint8_t ctrl = control[index];

      if (ctrl == EMPTY)
      {
        return std::nullopt;
      }
      else if (ctrl == H2)
      {
        const Entry &e = entries[index];
        if (e.key_len != key.length())
          continue;  // Length mismatch, not our key

        // For small keys (less than 8 bytes), compare using short_key
        if (e.key_len < 8)
        {
          uint64_t smol = 0;
          std::memcpy(&smol, key.data(), key.length());
          if (smol == e.short_key)
            return std::string(e.value, e.value_len);
          else
            continue;  // Not a match, keep searching
        }
        // For longer keys, do full comparison
        if (e.hash == hash && e.key[0] == key[0] && std::memcmp(e.key, key.data(), key.size()) == 0)
          return std::string(e.value, e.value_len);
      }
    }
    return std::nullopt;
  }

  // === Erase an entry by key ===
  bool erase(const std::string &key)
  {
    if (count == 0)
      return false;

    const uint64_t hash = hash_string(key.data(), key.size());
    const uint8_t H2 = (hash >> 57) & 0x7F;
    size_t start_index = hash & mask;

    for (size_t i = 0; i < capacity; i++)
    {
      size_t index = (start_index + i) & mask;
      uint8_t &ctrl = control[index];

      if (ctrl == EMPTY)
      {
        return false;
      }
      else if (ctrl == H2)
      {
        Entry &e = entries[index];
        if (e.hash == hash && e.key_len == key.size() && std::memcmp(e.key, key.data(), key.size()) == 0)
        {
          ctrl = DELETED;
          e = Entry{};  // Reset entry
          count--;
          return true;
        }
      }
    }
    return false;
  }

  // === Accessors ===
  size_t size() const { return count; }
  size_t get_capacity() const { return capacity; }
};
