load("@arm_none_eabi//:defs.bzl", "BIN_PREFIX", "GCC_VERSION", "TOOLCHAIN_DIR")
load(":arm_none_eabi_toolchain.bzl", "arm_none_eabi_toolchain")

arm_none_eabi_toolchain(
    name = "stm32g4_impl",
    bin_prefix = BIN_PREFIX,
    toolchain_dir = TOOLCHAIN_DIR,
    gcc_version = GCC_VERSION,
    cpu_flags = [
        "-mcpu=cortex-m4",
        "-mthumb",
        "-mfpu=fpv4-sp-d16",
        "-mfloat-abi=hard",
    ],
    toolchain_files = "@arm_none_eabi//:all_files",
)

toolchain(
    name = "arm_none_eabi_toolchain",
    exec_compatible_with = [],
    target_compatible_with = [
        "@platforms//os:none",
        "@platforms//cpu:armv7e-m",
    ],
    toolchain = ":stm32g4_impl",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)
