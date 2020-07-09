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

"""Blaze targets for golden file testing of the pdpi library.

This file defines targets `run_pdpi` and `diff_test`, which are intended to be
used in conjunction for "golden file testing" as follows:
```BUILD.bazel
    run_pdpi(
        name = "metadata",
        src = "testdata/metadata.pb.txt",
    )

    diff_test(
        name = "metadata_test",
        actual = ":metadata",
        expected = "testdata/metadata.expected"  # golden file
    )
```
The run_pdpi takes an input proto describing the different test cases, and
produces either the output or the error message of running on that test input.

The diff_test target then computes the diff of the `actual` output and the
`expected` output, either succeeding if the diff is empty or failing and
printing the nonempty diff otherwise. To auto-generate or update the expected
file, run:
```sh
    bazel run <diff test target> -- --update`
```
Make sure that the expected file exists.
"""

def execpath(path):
    return "$(execpath %s)" % path

def rootpath(path):
    return "$(rootpath %s)" % path

def run_pdpi(name, src, deps = [], visibility = None):
    """Runs pdpi_test_runner on the test cases given in the input file.

    Args:
      name: Name of this target.
      src: Protobuf describing the test cases.
      out: The output (stdin & sterr) is written to this file.
      visibility: Visibility of this target.
    """
    pdpi_test_runner = "//p4_pdpi/testing:pdpi_test_runner"
    native.genrule(
        name = name,
        visibility = visibility,
        srcs = [src] + deps,
        outs = [src + ".actual"],
        tools = [pdpi_test_runner],
        cmd = """
            "{pdpi_test_runner}" --tests=$(SRCS) &> $(OUTS) || true
        """.format(
            pdpi_test_runner = execpath(pdpi_test_runner),
        ),
    )

def _diff_test_script(ctx):
    """Returns bash script to be executed by the diff_test target."""
    return """
if [[ "$1" == "--update" ]]; then
    cp -f "{actual}" "${{BUILD_WORKSPACE_DIRECTORY}}/{expected}"
fi

diff -u "{expected}" "{actual}"

if [[ $? = 0 ]]; then
    # Expected and actual agree.
    if [[ "$1" == "--update" ]]; then
        echo "Successfully updated: {expected}. Contents:"
        echo ""
        cat {expected}
    else
        echo "PASSED"
    fi
    exit 0
else
    # Expected and actual disagree.
    if [[ "$1" == "--update" ]]; then
        echo "Failed to update: {expected}. Try updating manually."
    else
        cat << EOF

Output not as expected. To update $(basename {expected}), run the following command:
bazel run {target} -- --update
EOF
    fi
    exit 1
fi
    """.format(
        actual = ctx.file.actual.short_path,
        expected = ctx.file.expected.short_path,
        target = ctx.label,
    )

def _diff_test_impl(ctx):
    """Computes diff of two files, checking that they agree.

    When invoked as `bazel run <target> -- --update`, will update the `expected`
    file to match the contents of the `actual` file. Note that the file must
    already exist.
    """

    # Write test script that will be executed by 'bazel test'.
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = _diff_test_script(ctx),
    )

    # Make test script dependencies available at runtime.
    runfiles = [ctx.file.actual, ctx.file.expected]
    return DefaultInfo(
        runfiles = ctx.runfiles(files = runfiles),
    )

diff_test = rule(
    doc = """Computes diff of two files, checking that they agree.

    Typically used to test that the output of some command looks as expected.
    To update the expected file, run `bazel run <target> -- --update`.
    """,
    implementation = _diff_test_impl,
    test = True,
    attrs = {
        "actual": attr.label(
            doc = "'Actual' file, typically containing the output of some command.",
            mandatory = True,
            allow_single_file = True,
        ),
        "expected": attr.label(
            doc = """\
Expected file (aka golden file), containing the expected output.
To auto-generate or update, run `bazel run <target> -- --update`.
""",
            mandatory = True,
            allow_single_file = True,
        ),
    },
)
