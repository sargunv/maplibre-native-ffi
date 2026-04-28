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

fn addSdl3MapExample(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const sdl = b.dependency("sdl", .{
        .target = options.target,
        .optimize = options.optimize,
    });
    const zig_objc = b.dependency("zig_objc", .{
        .target = options.target,
        .optimize = options.optimize,
    });

    const example = b.addExecutable(.{
        .name = "zig-sdl3-map",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/zig-sdl3-map/main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreAbi(b, example.root_module, options.cmake_artifact_dir);
    example.root_module.addImport("objc", zig_objc.module("objc"));
    example.root_module.linkLibrary(sdl.artifact("SDL3"));
    example.root_module.linkFramework("Foundation", .{});
    example.root_module.linkFramework("Metal", .{});
    example.root_module.linkFramework("QuartzCore", .{});
    b.installArtifact(example);
    return example;
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
    return abi_tests;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
        .cmake_artifact_dir = cmakeArtifactDir(b),
    };

    const sdl3_map = addSdl3MapExample(b, options);
    const abi_tests = addAbiTests(b, options);

    const run_sdl3_map = b.addRunArtifact(sdl3_map);
    const run_step = b.step("run", "Run SDL3 Zig map example");
    run_step.dependOn(&run_sdl3_map.step);
    const run_sdl3_map_step = b.step("run:sdl3-map", "Run SDL3 Zig map example");
    run_sdl3_map_step.dependOn(&run_sdl3_map.step);

    const run_abi_tests = b.addRunArtifact(abi_tests);
    const test_step = b.step("test", "Run Zig ABI tests");
    test_step.dependOn(&run_abi_tests.step);
}
