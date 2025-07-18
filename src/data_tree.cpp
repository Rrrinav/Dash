#include "data_tree.hpp"
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
