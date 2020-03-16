#include <core.p4>
#include <v1model.p4>

struct metadata {
  bit<1> field1;
  bit<2> field2;
  bit<32> field3;
  bit<33> field4;
  bit<64> field5;
  bit<65> field6;
  bit<1024> field7;
}
struct headers {
}

// Note: proto_tag annotations are only necessary until PD supports the @id annotation, which will be preferred.

// Action with argumnt IDs changed
@id(0x01000001)
action action1(@id(2) @proto_tag(2) bit<32> arg1, @id(1) @proto_tag(1) bit<32> arg2) {
}

// Action with different argument types
@id(0x01000002)
action action2(@proto_tag(1) bit<1> arg1,
               @proto_tag(2) bit<2> arg2,
               @proto_tag(3) bit<32> arg3,
               @proto_tag(4) bit<33> arg4,
               @proto_tag(5) bit<64> arg5,
               @proto_tag(6) bit<65> arg6,
               @proto_tag(7) bit<1024> arg7) {
}

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {

  // Table with different field types
  @id(0x02000001)
  @proto_package("pdpi")
  table table1 {
    key = {
      meta.field1 : exact @proto_tag(1);
      meta.field2 : exact @proto_tag(2);
      meta.field3 : exact @proto_tag(3);
      meta.field4 : exact @proto_tag(4);
      meta.field5 : exact @proto_tag(5);
      meta.field6 : exact @proto_tag(6);
      meta.field7 : exact @proto_tag(7);
    }
    actions = {
      @proto_tag(1) action1;
      @proto_tag(2) action2;
      @proto_tag(3) NoAction();
    }
    default_action = NoAction();
  }

  // Table with field match ID annotations
  @id(0x02000002)
  @proto_package("pdpi")
  table table2 {
    key = {
      meta.field1 : exact @id(2) @proto_tag(2);
      meta.field2 : exact @id(1) @proto_tag(1);
    }
    actions = {
      @proto_tag(1) action1;
      @proto_tag(2) NoAction();
    }
    default_action = NoAction();
  }

  // Table with different match kinds
  @id(0x02000003)
  @proto_package("pdpi")
  table table3 {
    key = {
      meta.field1 : exact @proto_tag(1);
      meta.field2 : ternary @proto_tag(2);
      meta.field3 : lpm @proto_tag(3);
      // TODO: add optional once PD supports it
      // meta.field4 : optional;
    }
    actions = {
      @proto_tag(1) action1;
      @proto_tag(2) NoAction();
    }
    default_action = NoAction();
  }

  apply {
    table1.apply();
    table2.apply();
    table3.apply();
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
