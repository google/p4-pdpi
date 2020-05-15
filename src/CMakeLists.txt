
add_library(pdpi_ir util.cc meta.cc ir.cc)

target_link_libraries(pdpi_ir
	absl::strings
	p4runtime_proto_grpc)

