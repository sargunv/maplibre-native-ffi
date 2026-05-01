const std = @import("std");

const BuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cmake_artifact_dir: std.Build.LazyPath,
};

fn linkMapLibreC(b: *std.Build, module: *std.Build.Module, cmake_artifact_dir: std.Build.LazyPath) void {
    module.addIncludePath(b.path("include"));
    module.addLibraryPath(cmake_artifact_dir);
    module.addRPath(cmake_artifact_dir);
    module.linkSystemLibrary("maplibre-native-c", .{});
    module.link_libc = true;
}

fn cmakeArtifactDir(b: *std.Build) std.Build.LazyPath {
    const path = b.option(
        []const u8,
        "cmake-artifact-dir",
        "Directory containing the CMake-built maplibre-native-c library",
    ) orelse "build";

    if (std.fs.path.isAbsolute(path)) {
        return .{ .cwd_relative = path };
    }
    return b.path(path);
}

fn addCTests(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const c_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests/c/main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreC(b, c_tests.root_module, options.cmake_artifact_dir);
    if (options.target.result.os.tag == .macos) {
        c_tests.root_module.linkFramework("Metal", .{});
        c_tests.root_module.linkFramework("QuartzCore", .{});
    } else if (options.target.result.os.tag == .linux) {
        c_tests.root_module.addIncludePath(b.path("third_party/maplibre-native/vendor/Vulkan-Headers/include"));
        c_tests.root_module.addLibraryPath(b.path(".pixi/envs/default/lib"));
        c_tests.root_module.addRPath(b.path(".pixi/envs/default/lib"));
        c_tests.root_module.linkSystemLibrary("vulkan", .{});
    }
    return c_tests;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
        .cmake_artifact_dir = cmakeArtifactDir(b),
    };

    const c_tests = addCTests(b, options);

    const run_c_tests = b.addRunArtifact(c_tests);
    const test_step = b.step("test", "Run Zig C API tests");
    test_step.dependOn(&run_c_tests.step);
}
