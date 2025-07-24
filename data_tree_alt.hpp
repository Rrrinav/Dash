#pragma once

#include <algorithm>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "src/assert.hpp"

struct Node;
struct Leaf;

using Tree = std::variant<std::monostate, Node*, Leaf*>;
using Tag = uint8_t;

inline constexpr Tag TAG_ROOT = 1; // 0001
inline constexpr Tag TAG_NODE = 2; // 0010
inline constexpr Tag TAG_LEAF = 4; // 0100

using NodeID = std::uint32_t;

// Using RAM addresses as hash
class string_intern
{
  static inline std::vector<std::string> _paths;
  static inline std::unordered_map<std::string, NodeID> _str_to_id;
public:
  // Interns a string and returns its unique ID (1-based)
  [[nodiscard]]
  static NodeID string_to_key(const std::string &path)
  {
    auto it = _str_to_id.find(path);
    if (it != _str_to_id.end())
      return it->second;

    _paths.emplace_back(path);
    NodeID id = _paths.size();  // 1-based
    _str_to_id[path] = id;
    return id;
  }

  // Resolve an ID back to the original string
  [[nodiscard]]
  static std::optional<std::string_view> key_to_string(NodeID id)
  {
    if (id == 0 || id > _paths.size())
      return std::nullopt;
    return std::string_view(_paths[id - 1]);
  }
};

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

// Returns index where first id >= desired Id exists.
inline NodeID lower_bound(const std::vector<std::pair<uint64_t, Node*>>& nodes, NodeID id)
{
  auto it = std::ranges::lower_bound(nodes, id, {}, [](const auto& pair) {
    return pair.first;
  });
  return static_cast<NodeID>(it - nodes.begin());
}

struct Node
{
  Node *      _parent = nullptr;
  std::string _path;
  NodeID      _id;
  std::vector<std::pair<uint64_t, Node*>> _nodes;

  Node(Node * parent, const std::string& path)
    : _parent(parent), _path(path), _id(string_intern::string_to_key(path)), _nodes({})
  {}

  ~Node()
  {
    for (auto n : _nodes) delete n.second;
  }

  NodeID insert(Node * node)
  {
    __assert(node != nullptr, "Node has to exist");

    NodeID id = string_intern::string_to_key(node->_path);
    node->_id = id;
    std::size_t lb = lower_bound(_nodes, id);
    if (lb < _nodes.size() && _nodes[lb].first == id)
      return _nodes[lb].first;

    _nodes.insert(_nodes.begin() + lb, {id,  node});
    return id;
  }

  static Node * create_node(Node * parent, const std::string& path)
  {
    __assert(parent != nullptr, "Node has to exist");

    Node *node = new Node(parent, path);
    return node;
  }

  Node * create_child_node(const std::string& path)
  {
    NodeID id = string_intern::string_to_key(path);
    std::size_t lb = lower_bound(_nodes, id);
    if (lb < _nodes.size() && _nodes[lb].first == id)
      return _nodes[lb].second;

    // Doing heap allocation only if necessary, that's why searching earler.
    Node *node = new Node(this, path);
    node->_id = id;
    _nodes.insert(_nodes.begin() + lb, {id, node});
    return node;
  }

  std::optional<Node *> search(const std::string& path)
  {
    NodeID id = string_intern::string_to_key(path);

    std::size_t lb = lower_bound(_nodes, id);
    if (lb < _nodes.size() && _nodes[lb].first == id)
      return _nodes[lb].second;
    else
      return std::nullopt;
  }

  Node* delete_child_node(const std::string& path)
  {
    NodeID id = string_intern::string_to_key(path);

    std::size_t lb = lower_bound(_nodes, id);
    if (lb < _nodes.size() && _nodes[lb].first == id)
    {
      Node* child = _nodes[lb].second;
      _nodes.erase(_nodes.begin() + lb);
      return child;
    }
    return nullptr; // not found
  }
};


