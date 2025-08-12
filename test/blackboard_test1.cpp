#include "blackboard.hpp"
#include <iostream>

using msm::blackboard;
using msm::blackboard_entry;
using msm::blackboard_entry_interface;


auto main(int argc, char** argv) -> int {
  blackboard bb;

  bb.set<int>("int_key", 42);
  bb.set<double>("double_key", 3.14);
  bb.set<std::string>("string_key", "Hello, Blackboard!");  

  return 0;
}
