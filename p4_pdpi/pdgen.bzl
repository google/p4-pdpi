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
"""Rule for invoking the PD generator."""

def p4_pd_proto(name, src, out, package, format = True, visibility = None):
    """Generates PD proto from p4info file."""
    pdgen = "//p4_pdpi:pdgen"
    p4info = ":" + src
    tools = [pdgen]
    cmd = """
        $(location {pdgen})\\
            --p4info $(location {p4info})\\
            --package "{package}"\\
            > $(OUTS)
    """.format(
        p4info = p4info,
        package = package,
        pdgen = pdgen,
    )

    native.genrule(
        name = name,
        outs = [out],
        cmd = cmd,
        srcs = [p4info],
        tools = tools,
        visibility = visibility,
    )
