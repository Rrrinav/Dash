#include "data_tree.hpp"
#include <cstddef>
#include <variant>
#include "./assert.hpp"

Node::~Node()
{
  delete _left;
  Leaf *leaf = _right;
  while (leaf)
  {
    Leaf *next = leaf->_right;
    delete leaf;
    leaf = next;
  }
}

Node *Node::create_node(Node *parent, const std::string &path)
{
  __assert(parent, "Parent has to exist");

  Node *node = new Node(TAG_NODE, path, parent);
  parent->_left = node;
  return node;
}

inline Leaf *find_last_leaf(Node *parent)
{
  __assert(parent, "Parent has to exist");

  Leaf *l = parent->_right;
  while (l && l->_right)
    l = l->_right;
  return l;
}

Leaf *Leaf::create_leaf(Node *parent, const std::string &key, const std::string &value)
{
  __assert(parent, "Parent has to exist");

  Leaf *leaf = new Leaf(key, value);
  Leaf *last = find_last_leaf(parent);

  if (!last)
  {
    parent->_right = leaf;
    leaf->_left    = parent;
  }
  else
  {
    last->_right = leaf;
    leaf->_left  = last;
  }
  return leaf;
}

void pretty_print_impl(const Tree &t, int depth, const std::string &prefix, std::string &out)
{
  if (auto node = std::get_if<Node *>(&t); node && *node)
  {
    // Print node path
    out += std::string(depth * 2, ' ') + (*node)->_path + "\n";
    // For each leaf child of this node
    for (Leaf *leaf = (*node)->_right; leaf; leaf = leaf->_right)
      pretty_print_impl(leaf, depth + 2, (*node)->_path, out);

    if((*node)->_left)
      pretty_print_impl((*node)->_left, depth + 1, (*node)->_path + "/", out);
  }
  else if (auto leaf = std::get_if<Leaf *>(&t); leaf && *leaf)
  {
    // Full path = prefix + key
    std::string fullPath = prefix + (*leaf)->_key;
    // Print leaf with its value
    out += std::string(depth * 2, ' ') + fullPath + " -> " + (*leaf)->_value + "\n";
    for(Leaf * l = (*leaf)->_right; l; l = l->_right)
      pretty_print_impl(l, depth, prefix, out);
  }
  else
  {
    out = "Invalid tree : <null>";
  }
}

std::string pretty_print(const Tree &t, int depth)
{
  if (t.valueless_by_exception()) 
    return "<null>";

  std::string out;
  pretty_print_impl(t, depth, "", out);
  return out;
}
