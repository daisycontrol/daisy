load("@rules_python//python:defs.bzl", "py_library", "py_binary")
load("@rules_python//python:packaging.bzl", "py_wheel")

py_library(
    name = "daisycontrol_lib",
    srcs = glob(["daisycontrol/**/*.py"]),
    imports = ["."],
    visibility = ["//visibility:public"],
)

py_wheel(
    name = "daisycontrol_wheel",
    distribution = "daisycontrol",
    version = "0.0.1",
    python_tag = "py3",
    deps = [":daisycontrol_lib"],
    # packages = ["daisycontrol", "daisycontrol.bldc_drive"]
)

py_binary(
    name = "calibrate",
    srcs = ["calibrate.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
        "@pip//matplotlib",
    ]
)

py_binary(
    name = "run_test",
    srcs = ["run_test.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
        "@pip//matplotlib",
    ]
)


py_binary(
    name = "write_memory",
    srcs = ["write_memory.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
    ]
)

py_binary(
    name = "read_memory",
    srcs = ["read_memory.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
    ]
)

py_binary(
    name = "read_encoder",
    srcs = ["read_encoder.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
    ]
)

py_binary(
    name = "run_handle",
    srcs = ["run_handle.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
        "@pip//ruckig",
    ]
)

py_binary(
    name = "raw_voltage_test",
    srcs = ["raw_voltage_test.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        ":daisycontrol_lib",
        "@pip//python_can",
        "@pip//numpy",
        "@pip//pyserial",
        "@pip//ruckig",
    ]
)