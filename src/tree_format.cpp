#include "./data_tree.hpp"
#include <format>

std::string indent(int depth) { return std::string(depth * 2, ' '); }

std::string tag_string(Tag tag)
{
  std::string tags;
  if (tag & TAG_ROOT)
    tags += "ROOT|";
  if (tag & TAG_NODE)
    tags += "NODE|";
  if (tag & TAG_LEAF)
    tags += "LEAF|";
  if (!tags.empty() && tags.back() == '|')
    tags.pop_back();
  return tags;
}

std::string format_tree(const Tree &tree, int depth)
{
  return std::visit(
      [&](const auto &val) -> std::string
      {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Node*>)
        {
          if (val)
            return format_node(*val, depth);
          else
            return std::format("{}[Node] <null>\n", indent(depth));
        }
        else if constexpr (std::is_same_v<T, Leaf*>)
        {
          if (val)
            return format_leaf(*val, depth);
          else
            return std::format("{}[Leaf] <null>\n", indent(depth));
        }
        else
        {
          return std::format("{}[Unknown type]\n", indent(depth));
        }
      },
      tree);
}

std::string format_node(const Node &node, int depth)
{
  std::string out = std::format("{}[Node] path: '{}', tag: {}\n", indent(depth), node._path, tag_string(node._tag));
  if (node._right)
    out += format_leaf(*node._right, depth + 2);
  if (node._left)
    out += format_node(*node._left, depth + 1);
  return out;
}

std::string format_leaf(const Leaf &leaf, int depth)
{
  std::string out = std::format("{}[Leaf] key: '{}', value: '{}', tag: {}\n", indent(depth), leaf._key, leaf._value, tag_string(leaf._tag));
  if (leaf._right)
    out += format_leaf(*leaf._right, depth);  // Fixed: should call format_leaf, not format_tree
  return out;
}
