const std = @import("std");

const BuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
};

fn linkMapLibreC(module: *std.Build.Module) void {
    module.linkSystemLibrary("maplibre-native-c", .{ .use_pkg_config = .force });
    module.link_libc = true;
}

fn isSupportedTarget(target: std.Build.ResolvedTarget) bool {
    return target.result.os.tag == .macos or target.result.os.tag == .linux;
}

fn failUnsupportedTarget() noreturn {
    @panic("zig-map does not support this target platform");
}

fn addZigMapExample(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const example = b.addExecutable(.{
        .name = "zig-map",
        .root_module = b.createModule(.{
            .root_source_file = b.path("main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreC(example.root_module);
    example.root_module.addIncludePath(b.path("../../.pixi/envs/default/include"));
    example.root_module.addLibraryPath(b.path("../../.pixi/envs/default/lib"));
    example.root_module.addRPath(b.path("../../.pixi/envs/default/lib"));
    example.root_module.linkSystemLibrary("SDL3", .{});
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
        example.root_module.linkSystemLibrary("vulkan", .{});
    } else {
        failUnsupportedTarget();
    }
    b.installArtifact(example);
    return example;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
    };

    const run_step = b.step("run", "Run Zig map example");
    if (!isSupportedTarget(options.target)) {
        failUnsupportedTarget();
    }
    const zig_map = addZigMapExample(b, options);
    const run_zig_map = b.addRunArtifact(zig_map);
    run_step.dependOn(&run_zig_map.step);
}
