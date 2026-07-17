load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("@arm_none_eabi//:defs.bzl", "BIN_PREFIX")

cc_library(
    name = "ipc",
    srcs = glob(["ipc/*.cpp"]),
    hdrs = glob(["ipc/*.h"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "transform",
    srcs = glob(["transform/*.cpp"]),
    hdrs = glob(["transform/*.h"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "bmi270",
    srcs = glob(["bmi270/*.c"]),
    hdrs = glob(["bmi270/*.h"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "bmm150",
    srcs = glob(["bmm150/*.c"]),
    hdrs = glob(["bmm150/*.h"]),
    visibility = ["//visibility:public"],
)