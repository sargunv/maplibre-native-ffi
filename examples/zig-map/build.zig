const std = @import("std");

const BuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cmake_artifact_dir: std.Build.LazyPath,
};

fn cmakeArtifactDir(b: *std.Build) std.Build.LazyPath {
    const path = b.option(
        []const u8,
        "cmake-artifact-dir",
        "Directory containing the CMake-built maplibre_native_abi library",
    ) orelse "../../build";

    if (std.fs.path.isAbsolute(path)) {
        return .{ .cwd_relative = path };
    }
    return b.path(path);
}

fn linkMapLibreAbi(b: *std.Build, module: *std.Build.Module, cmake_artifact_dir: std.Build.LazyPath) void {
    module.addIncludePath(b.path("../../include"));
    module.addLibraryPath(cmake_artifact_dir);
    module.addRPath(cmake_artifact_dir);
    module.linkSystemLibrary("maplibre_native_abi", .{});
    module.link_libc = true;
}

fn addZigMapExample(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const sdl = b.dependency("sdl", .{
        .target = options.target,
        .optimize = options.optimize,
    });

    const example = b.addExecutable(.{
        .name = "zig-map",
        .root_module = b.createModule(.{
            .root_source_file = b.path("main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreAbi(b, example.root_module, options.cmake_artifact_dir);
    example.root_module.linkLibrary(sdl.artifact("SDL3"));
    if (options.target.result.os.tag == .macos) {
        const zig_objc = b.dependency("zig_objc", .{
            .target = options.target,
            .optimize = options.optimize,
        });
        example.root_module.addImport("objc", zig_objc.module("objc"));
        example.root_module.linkFramework("Foundation", .{});
        example.root_module.linkFramework("Metal", .{});
        example.root_module.linkFramework("QuartzCore", .{});
    } else if (options.target.result.os.tag == .linux) {
        example.root_module.addIncludePath(b.path("../../third_party/maplibre-native/vendor/Vulkan-Headers/include"));
        example.root_module.addLibraryPath(b.path("../../.pixi/envs/default/lib"));
        example.root_module.addRPath(b.path("../../.pixi/envs/default/lib"));
        example.root_module.linkSystemLibrary("vulkan", .{});
    }
    b.installArtifact(example);
    return example;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
        .cmake_artifact_dir = cmakeArtifactDir(b),
    };

    const run_step = b.step("run", "Run Zig map example");
    if (options.target.result.os.tag == .macos or options.target.result.os.tag == .linux) {
        const zig_map = addZigMapExample(b, options);
        const run_zig_map = b.addRunArtifact(zig_map);
        run_step.dependOn(&run_zig_map.step);
    }
}
