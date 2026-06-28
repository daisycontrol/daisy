load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_library")

def generate_messages(name, schema, deps = [], visibility = None):
    
    dep_schema_files = [d + "_schema_file" for d in deps]

    # C++
    native.genrule(
        name = name + "_gen_cpp",
        srcs = [schema] + dep_schema_files,
        outs = [name + ".h"],
        cmd = "$(location //schema/tools:generate_messages) --lang cpp --schema $(location %s) --out $(location %s)" % (schema, name + ".h"),
        tools = ["//schema/tools:generate_messages"]
    )

    cc_library(
        name = name + "_cpp",
        hdrs = [name + ".h"],
        includes = ["."],
        deps = [d + "_cpp" for d in deps],
        visibility = visibility,
    )

    # Python
    native.genrule(
        name = name + "_gen_py",
        srcs = [schema] + dep_schema_files,
        outs = [name + ".py"],
        cmd = "$(location //schema/tools:generate_messages) --lang python --schema $(location %s) --out $(location %s)" % (schema, name + ".py"),
        tools = ["//schema/tools:generate_messages"]
    )

    py_library(
        name = name + "_py",
        srcs = [name + ".py"],
        deps = [d + "_py" for d in deps],
        visibility = visibility
    )

    

    