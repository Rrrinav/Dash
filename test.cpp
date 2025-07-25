#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>

#include "./src/leaf_map.hpp"

#define TEST(cond)                                                                \
  do {                                                                            \
    if (!(cond))                                                                  \
    {                                                                             \
      std::cerr << "Test failed at line " << __LINE__ << ": " #cond << std::endl; \
      return 1;                                                                   \
    }                                                                             \
    else                                                                          \
    {                                                                             \
      std::cout << "Test passed: " #cond << std::endl;                            \
    }                                                                             \
  } while (0)

int main()
{
  std::string s;
  // Test 1: Basic functionality
  {
    leaf_map map;
    TEST(map.size() == 0);
    TEST(map.empty());

    // Insert some elements
    map.put("apple", "fruit");
    map.put("banana", "yellow");
    map.put("carrot", "vegetable");

    TEST(map.size() == 3);
    TEST(!map.empty());

    // Test get
    TEST(*map.get("apple") == "fruit");
    TEST(*map.get("banana") == "yellow");
    TEST(*map.get("carrot") == "vegetable");
    TEST(map.get("nonexistent") == nullptr);

    // Test contains
    TEST(map.contains("apple"));
    TEST(!map.contains("pear"));

    // Test get_or
    TEST(map.get_or("apple", "default") == "fruit");
    TEST(map.get_or("pear", "default") == "default");

    // Test overwrite
    map.put("apple", "red");
    TEST(*map.get("apple") == "red");
    TEST(map.size() == 3);  // Size shouldn't change
    map.put("", "");
    TEST(*map.get("") == "");
    map.put("", "kk");
    TEST(*map.get("") == "kk");
    map.put("kk", "");
    TEST(*map.get("kk") == "");
    s = *map.get_opt("apple");
  }
  TEST(s == "red");

  // Test 2: Erase functionality
  {
    leaf_map map;
    map.put("one", "1");
    map.put("two", "2");
    map.put("three", "3");

    TEST(map.erase("two"));
    TEST(!map.erase("two"));   // Already deleted
    TEST(!map.erase("four"));  // Never existed
    TEST(map.size() == 2);
    TEST(map.get("two") == nullptr);

    // Should still be able to insert after delete
    map.put("two", "II");
    TEST(*map.get("two") == "II");
    TEST(map.size() == 3);
  }

  // Test 3: Iterator functionality
  {
    leaf_map map;
    std::unordered_map<std::string, std::string> ref_map;

    // Insert some random data
    std::vector<std::pair<std::string, std::string>> test_data = {{"a", "1"}, {"b", "2"}, {"c", "3"}, {"d", "4"}, {"e", "5"},
                                                                  {"f", "6"}, {"g", "7"}, {"h", "8"}, {"i", "9"}, {"j", "10"}};

    for (const auto &p : test_data)
    {
      map.put(p.first, p.second);
      ref_map[p.first] = p.second;
    }

    // Test iterator counts all elements
    size_t count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) ++count;
    TEST(count == test_data.size());

    // Test iterator visits all elements
    std::unordered_map<std::string, std::string> visited;
    for (const auto &p : map) visited[p.first] = p.second;
    TEST(visited == ref_map);

    // Test iterator after erase
    map.erase("c");
    map.erase("g");
    ref_map.erase("c");
    ref_map.erase("g");

    visited.clear();
    for (const auto &p : map) visited[p.first] = p.second;
    TEST(visited == ref_map);
  }

  // Test 4: Rehashing and capacity
  {
    leaf_map map(4);  // Small initial size to force rehashing
    TEST(map.size() == 0);

    // Insert enough elements to trigger multiple rehashes
    for (int i = 0; i < 100; ++i)
    {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);
      map.put(key, value);
    }

    TEST(map.size() == 100);

    // Verify all elements are still accessible
    for (int i = 0; i < 100; ++i)
    {
      std::string key = "key" + std::to_string(i);
      std::string expected = "value" + std::to_string(i);
      TEST(*map.get(key) == expected);
    }

    // Test reserve
    map.reserve(1000);
    TEST(map.size() == 100);
    for (int i = 0; i < 100; ++i)
    {
      std::string key = "key" + std::to_string(i);
      std::string expected = "value" + std::to_string(i);
      TEST(*map.get(key) == expected);
    }
  }

  // Test 5: Edge cases
  {
    leaf_map map;

    // Empty string keys and values
    map.put("", "empty key");
    map.put("empty value", "");
    TEST(*map.get("") == "empty key");
    TEST(*map.get("empty value") == "");

    // Very long strings
    std::string long_key(1000, 'x');
    std::string long_value(10000, 'y');
    map.put(long_key, long_value);
    TEST(*map.get(long_key) == long_value);

    // Collision testing (assuming hash function is good, we can't easily force collisions)
    // But we can test that different keys with same first char work
    map.put("a1", "first");
    map.put("a2", "second");
    TEST(*map.get("a1") == "first");
    TEST(*map.get("a2") == "second");
  }

  // Test 7: Const correctness
  {
    leaf_map map;
    map.put("const", "test");

    const leaf_map &const_map = map;
    TEST(*const_map.get("const") == "test");
    TEST(const_map.get_or("const", "default") == "test");
    TEST(const_map.get_or("missing", "default") == "default");
    TEST(const_map.contains("const"));
    TEST(!const_map.contains("missing"));
    TEST(const_map.size() == 1);
    TEST(!const_map.empty());

    // Const iteration
    for (const auto &p : const_map)
    {
      TEST(p.first == "const");
      TEST(p.second == "test");
    }
  }

  std::cout << "All tests passed successfully!" << std::endl;
  return 0;
}
