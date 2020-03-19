#include <iostream>

#include "p4/config/v1/p4info.pb.h"

int main() {
  p4::config::v1::P4Info info;
  std::cout << "P4Info: " << info.DebugString() << std::endl;
  return 0;
}
