load("@rules_python//python:defs.bzl", "py_binary", "py_library")

py_library(
    name = "codegen_lib",
    srcs = ["codegen.py"], 
    visibility = ["//visibility:public"],
)

py_binary(
    name = "generate_messages",
    srcs = ["generate_messages.py"],
    deps = [":codegen_lib"],
    visibility = ["//visibility:public"],
)