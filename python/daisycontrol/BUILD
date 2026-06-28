load("@rules_python//python:defs.bzl", "py_library", "py_binary")
load("@rules_python//python:packaging.bzl", "py_wheel")

py_library(
    name = "daisycontrol_lib",
    srcs = glob(["daisycontrol/**/*.py"]),
    imports = ["."],
)

py_wheel(
    name = "daisycontrol_wheel",
    distribution = "daisycontrol",
    version = "0.0.1",
    python_tag = "py3",
    deps = [":daisycontrol_lib"]
)

py_binary(
    name = "message_test",
    srcs = ["message_test.py"],
    deps = [
        "//schema:common_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
    ]
)