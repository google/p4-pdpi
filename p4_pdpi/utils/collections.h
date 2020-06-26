#ifndef PDPI_COLLECTIONS_H
#define PDPI_COLLECTIONS_H

#include "p4_pdpi/utils/status_utils.h"

namespace pdpi {

// Returns a const copy of the value associated with a given key if it exists,
// or a status failure if it does not.
//
// WARNING: prefer FindOrNull if the value can be large to avoid the copy.
template <typename M>
StatusOr<const typename M::mapped_type> FindOrStatus(
    const M& m, const typename M::key_type& k) {
  auto it = m.find(k);
  if (it != m.end()) return it->second;
  return absl::NotFoundError("Key not found.");
}

// Returns a non-const non-null pointer of the value associated with a given key
// if it exists, or a status failure if it does not.
template <typename M>
StatusOr<typename M::mapped_type*> FindPtrOrStatus(
    M& m, const typename M::key_type& k) {
  auto it = m.find(k);
  if (it != m.end()) return &it->second;
  return absl::NotFoundError("Key not found.");
}

// Returns a const pointer of the value associated with a given key if it
// exists, or a nullptr if it does not.
template <typename M>
const typename M::mapped_type* FindOrNull(
    const M& m, const typename M::key_type& k) {
  const auto it = m.find(k);
  if (it != m.end()) return &(it->second);
  return nullptr;
}

// Returns a non-const pointer of the value associated with a given key if it
// exists, or a nullptr if it does not.
template <typename M>
typename M::mapped_type* FindOrNull(
    M& m, const typename M::key_type& k) {
  auto it = m.find(k);
  if (it != m.end()) return &(it->second);
  return nullptr;
}

}  // namespace pdpi

#endif  // PDPI_COLLECTIONS_H
