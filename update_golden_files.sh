#!/bin/bash
# Updates all golden files.

bazel run //p4_pdpi/testing:info_test -- --update
bazel run //p4_pdpi/testing:table_entry_test -- --update
bazel run //p4_pdpi/testing:table_entry_test2 -- --update
bazel run //p4_pdpi/testing:packet_io_test -- --update
bazel run //p4_pdpi/testing:rpc_test -- --update
bazel run //p4_pdpi/testing:main_pd_test -- --update
