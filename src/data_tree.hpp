#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <format>

struct Node;
struct Leaf;

using Tree = std::variant<std::monostate, Node*, Leaf*>;
using Tag = uint8_t;

inline constexpr Tag TAG_ROOT = 1;
inline constexpr Tag TAG_NODE = 2;
inline constexpr Tag TAG_LEAF = 4;

struct Leaf
{
  Tag          _tag;
  Tree         _left;
  Leaf *       _right = nullptr;
  std::string  _key;
  std::string  _value;

  Leaf(const std::string& key, const std::string &value)
    : _tag(TAG_LEAF), _left(),  _right(nullptr), _key(key), _value(value) {}

  Leaf(const std::string &key, const std::string &value, Tree left)
    : _tag(TAG_LEAF), _left(left), _key(key), _value(value) {}

  ~Leaf() = default;
  Leaf(const Leaf &) = delete;
  Leaf &operator=(const Leaf &) = delete;

  Leaf(Leaf &&) = default;
  Leaf &operator=(Leaf &&) = default;

  static Leaf *create_leaf(Node *parent, const std::string &key, const std::string &value);
};

struct Node
{
  Tag         _tag;
  Node *      _up    = nullptr;
  Node *      _left  = nullptr;
  Leaf *      _right = nullptr;
  std::string _path;

  Node(Tag tag, const std::string &path, Node *parent = nullptr) 
    : _tag(tag), _up(parent), _path(path) {}

  Node(Tag tag, const std::string &path, Node *parent, Node *left, Leaf *right)
    : _tag(tag), _up(parent), _left(left), _right(right), _path(path) {}

  ~Node();
  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  Node(Node &&) = default;
  Node &operator=(Node &&) = default;

  static Node *create_node(Node *parent, const std::string &path);
};

Leaf *find_last_leaf(Node *parent);

std::string format_node(const Node &node, int depth = 0);
std::string format_leaf(const Leaf &leaf, int depth = 0);
std::string format_tree(const Tree &tree, int depth = 0);

template <>
struct std::formatter<Tree>
{
  constexpr auto parse(std::format_parse_context &ctx)
  {
    return ctx.begin();  // No custom format options
  }
  auto format(const Tree &t, std::format_context &ctx) const
  {
    return std::format_to(ctx.out(), "{}", format_tree(t));
  }
};

template <>
struct std::formatter<Node>
{
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }
  auto format(const Node &node, std::format_context &ctx) const 
  {
    return std::format_to(ctx.out(), "{}", format_node(node)); 
  }
};

template <>
struct std::formatter<Leaf>
{
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }
  auto format(const Leaf &leaf, std::format_context &ctx) const 
  {
    return std::format_to(ctx.out(), "{}", format_leaf(leaf)); 
  }
};
