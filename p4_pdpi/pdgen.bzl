# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@com_github_p4lang_p4c//:bazel/p4_library.bzl", "p4_library")

def p4_pd_proto(name, src, out, package, visibility = None):
    # Run PD proto generator followed by the auto-formatter
    native.genrule(
        name = name,
        outs = [out],
        cmd = """
            $(location //p4_pdpi:pdgen) --p4info $(location :%s) --package "%s" > $(OUTS)
            # TODO(heule): enable once the clang-format version problem has been fixed.
            #clang-format -style=google -i $(OUTS)
            """ % (src, package),
        tools = ["//p4_pdpi:pdgen"],
        srcs = [
            ":" + src,
        ],
        visibility = visibility,
    )
