const std = @import("std");

const BuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
};

fn linkMapLibreC(module: *std.Build.Module) void {
    module.linkSystemLibrary("maplibre-native-c", .{ .use_pkg_config = .force });
    module.link_libc = true;
}

fn addReadbackExample(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const example = b.addExecutable(.{
        .name = "zig-readback",
        .root_module = b.createModule(.{
            .root_source_file = b.path("main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreC(example.root_module);
    example.root_module.addLibraryPath(b.path("../../.pixi/envs/default/lib"));
    example.root_module.addRPath(b.path("../../.pixi/envs/default/lib"));
    if (options.target.result.os.tag == .macos) {
        example.root_module.linkFramework("Metal", .{});
        example.root_module.linkFramework("QuartzCore", .{});
    } else if (options.target.result.os.tag == .linux) {
        example.root_module.addIncludePath(b.path("../../third_party/maplibre-native/vendor/Vulkan-Headers/include"));
        example.root_module.linkSystemLibrary("vulkan", .{});
    }
    b.installArtifact(example);
    return example;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
    };

    const readback = addReadbackExample(b, options);
    const run_readback = b.addRunArtifact(readback);
    if (b.args) |args| run_readback.addArgs(args);

    const run_step = b.step("run", "Render a map image to map.ppm");
    run_step.dependOn(&run_readback.step);
}
