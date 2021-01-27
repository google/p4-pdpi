// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "p4_pdpi/sequencing.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/optional.h"
#include "boost/graph/adjacency_list.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

using ::p4::v1::Update;
using ::p4::v1::WriteRequest;

// We require boost::in_degree, which requires bidirectionalS.
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>
    Graph;
// Boost uses integers to identify vertices.
typedef int64_t Vertex;

typedef std::tuple<std::string, std::string, std::string> ForeignKeyValue;

absl::optional<std::string> GetMatchFieldValue(
    const IrTableDefinition& ir_table_definition, const Update& update,
    const std::string& match_field) {
  int32_t match_field_id = ir_table_definition.match_fields_by_name()
                               .at(match_field)
                               .match_field()
                               .id();
  for (const auto& match : update.entity().table_entry().match()) {
    if (match.field_id() == match_field_id) {
      if (match.has_exact()) {
        return match.exact().value();
      } else if (match.has_optional()) {
        return match.optional().value();
      }
    }
  }
  return absl::nullopt;
}

// Builds the dependency graph between updates. An edge from n to m indicates
// that n must be sent in a batch before sending m.
Graph BuildDependencyGraph(const IrP4Info& info,
                           const std::vector<Update>& updates) {
  // Graph of size n.
  Graph graph(updates.size());

  // Build indices to map foreign keys to the set of updates of that key.
  absl::flat_hash_map<ForeignKeyValue, absl::flat_hash_set<Vertex>> indices;
  for (int i = 0; i < updates.size(); i++) {
    const Update& update = updates[i];
    const IrTableDefinition& ir_table_definition =
        info.tables_by_id().at(update.entity().table_entry().table_id());
    std::string update_table_name = ir_table_definition.preamble().alias();
    for (const auto& ir_foreign_key : info.foreign_keys()) {
      if (update_table_name == ir_foreign_key.table()) {
        absl::optional<std::string> value = GetMatchFieldValue(
            ir_table_definition, update, ir_foreign_key.match_field());
        if (value.has_value()) {
          ForeignKeyValue foreign_key_value = {ir_foreign_key.table(),
                                               ir_foreign_key.match_field(),
                                               value.value()};
          indices[foreign_key_value].insert(i);
        }
      }
    }
  }

  // Build dependency graph.
  for (int update_index = 0; update_index < updates.size(); update_index++) {
    const Update& update = updates[update_index];
    const p4::v1::TableAction& action = update.entity().table_entry().action();
    const IrActionDefinition ir_action =
        info.actions_by_id().at(action.action().action_id());
    for (const auto& param : action.action().params()) {
      for (const auto& ir_foreign_key :
           ir_action.params_by_id().at(param.param_id()).foreign_keys()) {
        ForeignKeyValue foreign_key_value = {ir_foreign_key.table(),
                                             ir_foreign_key.match_field(),
                                             param.value()};
        for (Vertex referred_update_index : indices[foreign_key_value]) {
          const Update& referred_update = updates[referred_update_index];
          if ((update.type() == p4::v1::Update::INSERT ||
               update.type() == p4::v1::Update::MODIFY) &&
              referred_update.type() == p4::v1::Update::INSERT) {
            boost::add_edge(referred_update_index, update_index, graph);
          } else if (update.type() == p4::v1::Update::DELETE &&
                     referred_update.type() == p4::v1::Update::DELETE) {
            boost::add_edge(update_index, referred_update_index, graph);
          }
        }
      }
    }
  }
  return graph;
}

std::vector<p4::v1::WriteRequest> SequenceP4Updates(
    const IrP4Info& info, const std::vector<Update>& updates) {
  Graph graph = BuildDependencyGraph(info, updates);

  std::vector<Vertex> roots;
  for (Vertex vertex : graph.vertex_set()) {
    if (boost::in_degree(vertex, graph) == 0) {
      roots.push_back(vertex);
    }
  }

  std::vector<WriteRequest> requests;
  while (!roots.empty()) {
    // New write request for the current roots.
    WriteRequest request;
    for (Vertex root : roots) {
      *request.add_updates() = updates[root];
    }
    requests.push_back(request);

    // Remove edges for old roots and add new roots.
    absl::flat_hash_set<Vertex> new_root_candidates;
    for (Vertex root : roots) {
      std::vector<std::pair<Vertex, Vertex>> edges_to_remove;
      for (const auto& edge : graph.out_edge_list(root)) {
        edges_to_remove.push_back({root, edge.get_target()});
        new_root_candidates.insert(edge.get_target());
      }
      for (const auto& edge : edges_to_remove) {
        // Separate loop because remove_edge invalidates iterators.
        boost::remove_edge(edge.first, edge.second, graph);
      }
    }
    roots.clear();
    for (Vertex root_candidate : new_root_candidates) {
      if (boost::in_degree(root_candidate, graph) == 0) {
        roots.push_back(root_candidate);
      }
    }
  }
  return requests;
}

}  // namespace pdpi
