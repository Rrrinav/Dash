#include <print>

#include "./src/data_tree.hpp"

int main()
{
  Node root = Node(TAG_ROOT, "/");
  Node * n  = Node::create_node(&root, "/users/");
  Node * n1 = Node::create_node(n, "/users/login/");

  Leaf::create_leaf(n1, "Jonas", "abc001");
  Leaf::create_leaf(n1, "Jonas2", "abc002");

  Node * n2 = Node::create_node(n1, "/users/search/");
  Leaf::create_leaf(n2, "Jonas3", "abc003");
  Leaf::create_leaf(n2, "Jonas4", "abc004");

  std::println("{}", (Tree)(Node *)nullptr);
  std::println("{}", pretty_print((Node *)nullptr, 2));
}
