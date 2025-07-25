# Map Safety Guide

## Safe Usage Patterns

### 1. Immediate Copy (Always Safe)

```cpp
leaf_map map;
map.put("fruit", "apple");

// Safe - copies string immediately
std::string s = *map.get_opt("fruit"); 
```

### 2. Scope-Limited Reference

```cpp
{
    leaf_map map;
    map.put("color", "red");

    // Safe - reference used immediately
    if (auto opt = map.get_opt("color")) {
        opt->get() += "!"; // Safe modification
    }
}
```

### 3. Value Return API

```cpp
// Safest for functions
std::optional<std::string> safe_get(const leaf_map& m, const std::string& k) {
    if (auto opt = m.get_opt(k)) {
        return std::string(opt->get()); // Explicit copy
    }
    return std::nullopt;
}
```

## Dangerous Patterns

### Storing References

```cpp
std::reference_wrapper<std::string> danger;
{
    leaf_map temp;
    temp.put("tmp", "value");
    danger = *temp.get_opt("tmp"); // Dangling reference!
}
// danger.get() is now invalid
```

### Delayed Usage

```cpp
auto opt = map.get_opt("key");
map.put("new", "value"); // May rehash
std::string s = *opt;    // UNSAFE - map changed
```

## Key Rules

1. **Copy immediately** if you need the value to persist
2. **Use references only** within the map's lifetime
3. **Never store** reference_wrappers
4. **Any map modification** may invalidate references
