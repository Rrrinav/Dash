#pragma once

#include <algorithm>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <ranges>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "leaf_map.hpp"
#include "assert.hpp"

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

struct Node
{
  Tag        _tag;
  Node *      _parent = nullptr;
  std::string _path;
  NodeID      _id;
  std::vector<std::pair<uint64_t, Node*>> _nodes;
  Leaf_map _leaves;

  Node(Node * parent, const std::string& path)
    : _parent(parent), _path(path), _id(string_intern::string_to_key(path)), _nodes({})
  {}

  ~Node()
  {
    for (auto n : _nodes) delete n.second;
  }

  NodeID insert(Node * node);

  static Node * create_node(Node * parent, const std::string& path)
  {
    __assert(parent != nullptr, "Node has to exist");

    Node *node = new Node(parent, path);
    return node;
  }

  Node * create_child_node(const std::string& path);

  std::optional<Node *> search(const std::string& path);

  Node* delete_child_node(const std::string& path);
};

class Tree
{
  Node * _root;

public:
  Tree(const std::string& parth = "/") : _root(new Node(nullptr, "/"))
  { _root->_tag = TAG_ROOT; }
  ~Tree() { delete _root; }

  std::optional<Node *> find(const std::string& path) const;
  Node *insert(const std::string &path);

  bool remove(const std::string &path);

  std::expected<std::string *, std::string> set(const std::string& path, const std::string& key, const std::string& val);

  [[nodiscard("Use it immediately or copy it, pointer may become invalid after next operation on map or map deletion")]]
  std::expected<std::string*, std::string> get(const std::string& path, const std::string& key);

  std::string print() const;

private:
  void print_recursive(Node *root, int indent, std::string &out) const;

  std::vector<std::string> split_path_view(const std::string &path) const;

  std::string join_path(const std::vector<std::string> &components) const;
};
