#ifndef PDPI_PDPI_H
#define PDPI_PDPI_H

#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"

namespace pdpi {

constexpr char kFieldMatchFieldname[] = "match";
constexpr char kActionFieldname[] = "action";

constexpr char kLpmValueFieldname[] = "value";
constexpr char kLpmPrefixLenFieldname[] = "prefix_len";

constexpr char kTernaryValueFieldname[] = "value";
constexpr char kTernaryMaskFieldname[] = "mask";

void PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                      const p4::v1::TableEntry &pi,
                      google::protobuf::Message *pd);
}  // namespace pdpi

#endif  // PDPI_PDPI_H
