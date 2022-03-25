
def _py_dist_transition_impl(settings, attr):
    _ignore = (settings)
    return [{"//command_line_option:cpu": cpu} for cpu in attr.cpus]

_py_dist_transition = transition(
    implementation = _py_dist_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:cpu"]
)

def _py_dist_impl(ctx):
    return [
        DefaultInfo(files = depset(
            transitive = [dep[DefaultInfo].files for dep in ctx.attr.wheel],
        )),
    ]

py_dist = rule(
    implementation = _py_dist_impl,
    attrs = {
        "cpus": attr.string_list(
            mandatory = True,
        ),
        "wheel": attr.label(
            mandatory = True,
            cfg = _py_dist_transition,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist"
        ),
    }
    
)
