#ifndef PDPI_IR_H
#define PDPI_IR_H
// P4 intermediate representation proto definitions for use in conversion from
// Program-Independent to either Program-Dependent or App-DB formats

#include <iostream>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "p4/v1/p4runtime.pb.h"

namespace pdpi {
struct IrTernaryMatch {
  std::string value;
  std::string mask;
};

struct IrMatch {
  std::string name;
  absl::variant<std::string, IrTernaryMatch> value;
};

struct IrActionParam {
  std::string name;
  std::string value;
};

struct IrAction {
  std::string name;
  std::vector<IrActionParam> params;
};

struct IrTableEntry {
  std::string table_name;
  std::vector<IrMatch> matches;
  absl::optional<IrAction> action;
  absl::optional<int> priority;
  std::string controller_metatdata;
};

// Converts a PI table entry to the IR. Throws std::invalid_argument if PI is
// not well-formed, and throws a pdpi::internal_error exception (which is a
// std::runtime_error) on internal errors.
// Not implemented yet
// IrTableEntry PiToIr(const P4InfoMetadata &metadata,
//                     const p4::v1::TableEntry& pi);

// Converts an IR table entry to the PI representation. Throws
// std::invalid_argument if PI is not well-formed, and throws a
// pdpi::internal_error exception (which is a std::runtime_error) on internal
// errors.
// Not implemented yet
// p4::v1::TableEntry IrToPi(const P4InfoMetadata &metadata,
//                          const IrTableEntry& ir);
}  // namespace pdpi
#endif  // PDPI_IR_H
