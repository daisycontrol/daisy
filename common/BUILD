load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("@arm_none_eabi//:defs.bzl", "BIN_PREFIX")

cc_library(
    name = "ipc",
    srcs = glob(["ipc/*.cpp"]),
    hdrs = glob(["ipc/*.h"]),
    visibility = ["//visibility:public"],
)