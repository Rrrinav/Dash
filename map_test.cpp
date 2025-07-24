#include <cassert>
#include <iostream>
#include <string>
#include "./src/leaf_map.hpp"

void test_basic_insert_and_get()
{
  Leaf_map map;
  map.put("apple", "red");
  map.put("banana", "yellow");
  map.put("grape", "purple");

  assert(map.get("apple").value() == "red");
  assert(map.get("banana").value() == "yellow");
  assert(map.get("grape").value() == "purple");
  assert(!map.get("orange").has_value());
  std::cout << " test_basic_insert_and_get passed\n";
}

void test_overwrite()
{
  Leaf_map map;
  map.put("key", "value1");
  map.put("key", "value2");  // Overwrite

  assert(map.get("key").value() == "value2");
  std::cout << " test_overwrite passed\n";
}

void test_probe_and_robinhood()
{
  Leaf_map map;
  // Force collisions
  for (int i = 0; i < 100; ++i) map.put("key" + std::to_string(i), "val" + std::to_string(i));

  // Verify all entries
  for (int i = 0; i < 100; ++i) assert(map.get("key" + std::to_string(i)).value() == "val" + std::to_string(i));
  std::cout << " test_probe_and_robinhood passed\n";
}

void test_long_keys_and_values()
{
  Leaf_map map;
  std::string long_key(1000, 'k');
  std::string long_value(1000, 'v');
  map.put(long_key, long_value);

  assert(map.get(long_key).value() == long_value);
  std::cout << " test_long_keys_and_values passed\n";
}

void test_erase()
{
  Leaf_map map;
  map.put("k1", "v1");
  map.put("k2", "v2");

  assert(map.erase("k1") == true);
  assert(!map.get("k1").has_value());
  assert(map.size() == 1);

  // Erase non-existent key
  assert(map.erase("k3") == false);
  std::cout << " test_erase passed\n";
}

void test_empty_and_short_strings()
{
  Leaf_map map;
  map.put("", "empty_key");
  map.put("k", "short_value");

  assert(map.get("").value() == "empty_key");
  assert(map.get("k").value() == "short_value");
  std::cout << " test_empty_and_short_strings passed\n";
}

void test_colliding_keys()
{
  Leaf_map map;
  // These keys may collide depending on hash function
  map.put("abc", "val1");
  map.put("abd", "val2");
  map.put("abe", "val3");

  assert(map.get("abc").value() == "val1");
  assert(map.get("abd").value() == "val2");
  assert(map.get("abe").value() == "val3");
  std::cout << " test_colliding_keys passed\n";
}

void test_rehash_growth()
{
  Leaf_map map;
  size_t initial_cap = map.get_capacity();

  // Trigger multiple rehashes
  for (int i = 0; i < 1000; ++i) map.put("key" + std::to_string(i), "val" + std::to_string(i));

  assert(map.get_capacity() > initial_cap);
  for (int i = 0; i < 1000; ++i) assert(map.get("key" + std::to_string(i)).has_value());
  std::cout << " test_rehash_growth passed\n";
}

void test_erase_all()
{
  Leaf_map map;
  map.put("k1", "v1");
  map.put("k2", "v2");
  map.put("k3", "v3");

  map.erase("k1");
  map.erase("k2");
  map.erase("k3");

  assert(map.size() == 0);
  assert(!map.get("k1").has_value());
  std::cout << " test_erase_all passed\n";
}

void test_deleted_markers()
{
  Leaf_map map;
  map.put("key", "val1");
  map.erase("key");
  map.put("key", "val2");  // Reuse deleted slot

  assert(map.get("key").value() == "val2");
  std::cout << " test_deleted_markers passed\n";
}

void test_substring_keys()
{
  Leaf_map map;
  map.put("apple", "fruit");
  map.put("app", "short");

  assert(map.get("app").value() == "short");
  assert(map.get("apple").value() == "fruit");
  std::cout << " test_substring_keys passed\n";
}

void test_duplicate_overwrite()
{
  Leaf_map map;
  for (int i = 0; i < 10; ++i) map.put("key", "val" + std::to_string(i));
  assert(map.get("key").value() == "val9");
  std::cout << " test_duplicate_overwrite passed\n";
}

void test_mixed_operations()
{
  Leaf_map map;
  // Track explicitly which keys should exist
  std::vector<bool> should_exist(100, true);

  for (int i = 0; i < 100; ++i) {
    map.put("key" + std::to_string(i), "val" + std::to_string(i));
    if (i % 10 == 0) {
      int key_to_erase = i/2;
      map.erase("key" + std::to_string(key_to_erase));
      should_exist[key_to_erase] = false;
    }
  }

  // Verify based on tracked state
  for (int i = 0; i < 100; ++i) {
    if (should_exist[i]) {
      assert(map.get("key" + std::to_string(i)).has_value());
    } else {
      assert(!map.get("key" + std::to_string(i)).has_value());
    }
  }
  std::cout << " test_mixed_operations passed\n";
}

int main()
{
  test_basic_insert_and_get();
  test_overwrite();
  test_probe_and_robinhood();
  test_long_keys_and_values();
  test_erase();
  test_empty_and_short_strings();
  test_colliding_keys();
  test_rehash_growth();
  test_erase_all();
  test_deleted_markers();
  test_substring_keys();
  test_duplicate_overwrite();
  test_mixed_operations();

  std::cout << " All Leaf_map tests passed.\n";
  return 0;
}
