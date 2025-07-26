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

#include "data_tree.hpp"

#include "assert.hpp"

struct Node;
// Returns index where first id >= desired Id exists.
inline NodeID lower_bound(const std::vector<std::pair<uint64_t, Node *>> &nodes, NodeID id)
{
  auto it = std::ranges::lower_bound(nodes, id, {}, [](const auto &pair) { return pair.first; });
  return static_cast<NodeID>(it - nodes.begin());
}

NodeID Node::insert(Node *node)
{
  __assert(node != nullptr, "Node has to exist");

  NodeID id = string_intern::string_to_key(node->_path);
  node->_id = id;
  std::size_t lb = lower_bound(_nodes, id);
  if (lb < _nodes.size() && _nodes[lb].first == id)
    return _nodes[lb].first;

  _nodes.insert(_nodes.begin() + lb, {id, node});
  return id;
}

Node * Node::create_child_node(const std::string &path)
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

std::optional<Node *> Node::search(const std::string &path)
{
  NodeID id = string_intern::string_to_key(path);

  std::size_t lb = lower_bound(_nodes, id);
  if (lb < _nodes.size() && _nodes[lb].first == id)
    return _nodes[lb].second;
  else
    return std::nullopt;
}

Node * Node::delete_child_node(const std::string &path)
{
  NodeID id = string_intern::string_to_key(path);

  std::size_t lb = lower_bound(_nodes, id);
  if (lb < _nodes.size() && _nodes[lb].first == id)
  {
    Node *child = _nodes[lb].second;
    _nodes.erase(_nodes.begin() + lb);
    return child;
  }
  return nullptr;  // not found
}


std::optional<Node *> Tree::find(const std::string &path) const
{
  std::vector<std::string> comps = split_path_view(path);
  Node *current = this->_root;

  for (const auto &c : comps)
    if (auto found = current->search(c); found.has_value())
      current = found.value();
    else
      return std::nullopt;
  return current;
}

Node * Tree::insert(const std::string &path)
{
  std::vector<std::string> comps = split_path_view(path);
  Node *current = this->_root;

  for (const auto &c : comps)
    if (auto found = current->search(c); found)
      current = *found;
    else
      current = current->create_child_node(c);
  return current;
}

bool Tree::remove(const std::string &path)
{
  auto components = split_path_view(path);
  if (components.empty())
    return false;

  // Find parent of the node to remove
  std::string_view leaf = components.back();
  components.pop_back();

  auto parent = find(join_path(components));
  if (!parent)
    return false;

  Node *to_delete = (*parent)->delete_child_node(leaf.data());
  if (to_delete)
  {
    delete to_delete;
    return true;
  }
  return false;
}

std::expected<std::string *, std::string> Tree::set(const std::string &path, const std::string &key, const std::string &val)
{
  auto node = this->find(path);
  if (!node.has_value())
    return std::unexpected<std::string>(
        std::format("Couldn't set value: {} at key: {} because no node at path: {} exists", val, key, path));

  return node.value()->_leaves.put(key, val);
}

[[nodiscard("Use it immediately or copy it, pointer may become invalid after next operation on map or map deletion")]]
std::expected<std::string *, std::string> Tree::get(const std::string &path, const std::string &key)
{
  auto node = this->find(path);
  if (!node)
    return std::unexpected<std::string>(std::format("Couldn't get value at key: {} because no node at path: {} exists", key, path));

  auto v = node.value()->_leaves.get(key);
  if (v == nullptr)
    return std::unexpected<std::string>(
        std::format("Couldn't get value at key: {} & path: {} because key itself doesn't exist", key, path));

  return v;
}

std::string Tree::print() const
{
  std::string s{};
  print_recursive(_root, 0, s);
  return s;
}

void Tree::print_recursive(Node *root, int indent, std::string &out) const
{
  out += std::string(indent, ' ') + root->_path + "\n";

  if (!root->_leaves.empty())
    for (const auto &[k, v] : root->_leaves) out += std::string(indent + 1, ' ') + root->_path + " : " + k + " -> " + v + "\n";
  for (const auto &[id, child] : root->_nodes) print_recursive(child, indent + 2, out);
}

std::vector<std::string> Tree::split_path_view(const std::string &path) const
{
  using namespace std::literals;

  if (path.empty())
    return {};

  auto components = path | std::views::split('/') |
                    std::views::transform([](auto &&range) { return std::string(range.begin(), range.end()); }) |
                    std::views::filter([](const std::string &sv) { return !sv.empty(); });

  return {components.begin(), components.end()};
}

std::string Tree::join_path(const std::vector<std::string> &components) const
{
  if (components.empty())
    return "";

  std::string result;
  for (size_t i = 0; i < components.size(); ++i)
  {
    if (i != 0)
      result += '/';
    result += components[i].data();
  }
  return result;
}
