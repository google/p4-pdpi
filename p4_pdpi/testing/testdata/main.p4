#include <core.p4>
#include <v1model.p4>

@p4runtime_translation("", string)
type bit<12> string_id_t;

// Note: no format annotations, since these don't affect anything
struct metadata {
  bit<10> normal;
  bit<32> ipv4;
  bit<128> ipv6;
  bit<48> mac;
  string_id_t str;
}
struct headers {
}

@controller_header("packet_in")
header packet_in_header_t {
  // The port the packet ingressed on.
  @id(1)
  bit<10> ingress_port;
  // The initial intended egress port decided for the packet by the pipeline.
  @id(2)
  string_id_t target_egress_port;
}

@controller_header("packet_out")
header packet_out_header_t {
  // The port this packet should egress out of.
  @id(1)
  string_id_t egress_port;
  // Should the packet be submitted to the ingress pipeline instead of being
  // sent directly?
  @id(2)
  bit<1> submit_to_ingress;
}

// Note: proto_tag annotations are only necessary until PD supports the @id annotation, which will be preferred.

// Action with argumnt IDs changed
@id(0x01000001)
action action1(@id(2) bit<32> arg1, @id(1) bit<32> arg2) {
}

// Action with different argument types
@id(0x01000002)
action action2(@id(1) bit<10> normal,
               @id(2) @format(IPV4_ADDRESS) bit<32> ipv4,
               @id(3) @format(IPV6_ADDRESS) bit<128> ipv6,
               @id(4) @format(MAC_ADDRESS) bit<48> mac,
               @id(5) string_id_t str) {
}

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {

  bit<10> wcmp_selector_input = 0;

  // Table with field match ID annotations
  @id(0x02000001)
  @proto_package("pdpi")
  table id_test_table {
    key = {
      meta.ipv4 : exact @id(2) @format(IPV4_ADDRESS) @name("ipv4");
      meta.ipv6 : exact @id(1) @format(IPV6_ADDRESS) @name("ipv6");
    }
    actions = {
      @proto_id(2) action1;
      @proto_id(1) action2;
      @defaultonly NoAction();
    }
    default_action = NoAction();
  }

  // Table with exact matches
  @id(0x02000002)
  @proto_package("pdpi")
  table exact_table {
    key = {
      meta.normal : exact @id(1) @name("normal");
      meta.ipv4 : exact @id(2) @format(IPV4_ADDRESS) @name("ipv4");
      meta.ipv6 : exact @id(3) @format(IPV6_ADDRESS) @name("ipv6");
      meta.mac : exact @id(4) @format(MAC_ADDRESS) @name("mac");
      meta.str : exact @id(5) @name("str");
    }
    actions = {
      @proto_id(1) NoAction();
    }
    default_action = NoAction();
  }

  // Table with ternary matches
  @id(0x02000003)
  @proto_package("pdpi")
  table ternary_table {
    key = {
      meta.normal : ternary @id(1) @name("normal");
      meta.ipv4 : ternary @id(2) @format(IPV4_ADDRESS) @name("ipv4");
      meta.ipv6 : ternary @id(3) @format(IPV6_ADDRESS) @name("ipv6");
      meta.mac : ternary @id(4) @format(MAC_ADDRESS) @name("mac");
      meta.str : ternary @id(5) @name("str");
    }
    actions = {
      @proto_id(1) NoAction();
    }
    default_action = NoAction();
  }

  // Table with lpm matches
  @id(0x02000004)
  @proto_package("pdpi")
  table lpm1_table {
    key = {
      meta.ipv4 : lpm @id(1) @format(IPV4_ADDRESS) @name("ipv4");
    }
    actions = {
      @proto_id(1) NoAction();
    }
    default_action = NoAction();
  }

  // Table with lpm matches
  @id(0x02000005)
  @proto_package("pdpi")
  table lpm2_table {
    key = {
      meta.ipv6 : lpm @id(1) @format(IPV6_ADDRESS) @name("ipv6");
    }
    actions = {
      @proto_id(1) NoAction();
    }
    default_action = NoAction();
  }

  action_selector(HashAlgorithm.identity, 1024, 10) wcmp_group_selector;

  // WCMP table
  @id(0x02000006)
  @proto_package("pdpi")
  @oneshot()
  @weight_proto_id(1)
  table wcmp_table {
    key = {
      meta.ipv4 : lpm @id(1) @format(IPV4_ADDRESS) @name("ipv4");
      wcmp_selector_input : selector;
    }
    actions = {
      @proto_id(2) action1;
    }
    implementation = wcmp_group_selector;
  }

  apply {
    id_test_table.apply();
    exact_table.apply();
    ternary_table.apply();
    lpm1_table.apply();
    lpm2_table.apply();
    wcmp_table.apply();
  }
}

// Boilerplate definitions that are required for v1model, but do not affect the
// P4Info file (and thus do not matter for PDPI).

parser packet_parser(packet_in packet, out headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}
control checksum_verify(inout headers hdr, inout metadata meta) {
  apply {}
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t tandard_metadata) {
  apply {}
}
control checksum_compute(inout headers hdr, inout metadata meta) {
  apply {}
}
control packet_deparser(packet_out packet, in headers hdr) {
  apply {}
}
V1Switch(
  packet_parser(),
  checksum_verify(),
  ingress(),
  egress(),
  checksum_compute(),
  packet_deparser()
) main;
