load("@rules_python//python:defs.bzl", "py_library", "py_binary")
load("@rules_python//python:packaging.bzl", "py_wheel")

py_binary(
    name = "zmq_joystick",
    srcs = ["zmq_joystick.py"],
    deps = [
        "@pip//zmq",
        "@pip//pygame",
    ]
)


py_library(
    name = "handle_lib",
    srcs = glob(["remote_joystick.py"]),
    imports = ["."],
)


py_binary(
    name = "suction_gripper_test",
    srcs = ["suction_gripper_test.py"],
    deps = [
        "//schema:common_messages_py",
        "//hardware/bldc_drive:bldc_drive_messages_py",
        "//python/daisycontrol:daisycontrol_lib",
        ":handle_lib",
        "@pip//zmq",
        "@pip//python_can",
        "@pip//pyserial",    
    ]
)
