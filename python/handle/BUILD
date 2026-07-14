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



py_binary(
    name = "zmq_joystick_recv",
    srcs = ["zmq_joystick_recv.py"],
    deps = [
        "@pip//zmq",
    ]
)
