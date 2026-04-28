const std = @import("std");

const BuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cmake_artifact_dir: std.Build.LazyPath,
};

fn linkMapLibreAbi(b: *std.Build, module: *std.Build.Module, cmake_artifact_dir: std.Build.LazyPath) void {
    module.addIncludePath(b.path("include"));
    module.addLibraryPath(cmake_artifact_dir);
    module.addRPath(cmake_artifact_dir);
    module.linkSystemLibrary("maplibre_native_abi", .{});
    module.link_libc = true;
}

fn cmakeArtifactDir(b: *std.Build) std.Build.LazyPath {
    const path = b.option(
        []const u8,
        "cmake-artifact-dir",
        "Directory containing the CMake-built maplibre_native_abi library",
    ) orelse "build";

    if (std.fs.path.isAbsolute(path)) {
        return .{ .cwd_relative = path };
    }
    return b.path(path);
}

fn addAbiTests(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const abi_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests/abi/main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreAbi(b, abi_tests.root_module, options.cmake_artifact_dir);
    if (options.target.result.os.tag == .linux) {
        abi_tests.root_module.addIncludePath(b.path("third_party/maplibre-native/vendor/Vulkan-Headers/include"));
        abi_tests.root_module.addLibraryPath(b.path(".pixi/envs/default/lib"));
        abi_tests.root_module.addRPath(b.path(".pixi/envs/default/lib"));
        abi_tests.root_module.linkSystemLibrary("vulkan", .{});
    }
    return abi_tests;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
        .cmake_artifact_dir = cmakeArtifactDir(b),
    };

    const abi_tests = addAbiTests(b, options);

    const run_abi_tests = b.addRunArtifact(abi_tests);
    const test_step = b.step("test", "Run Zig ABI tests");
    test_step.dependOn(&run_abi_tests.step);
}
