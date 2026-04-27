const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const headless = b.addExecutable(.{
        .name = "headless_zig_example",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/zig-headless/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    headless.root_module.addIncludePath(b.path("include"));
    headless.root_module.addLibraryPath(b.path("build"));
    headless.root_module.addRPath(b.path("build"));
    headless.root_module.linkSystemLibrary("maplibre_native_abi", .{});
    headless.root_module.link_libc = true;

    b.installArtifact(headless);

    const run_headless = b.addRunArtifact(headless);
    const run_step = b.step("run", "Run headless Zig smoke example");
    run_step.dependOn(&run_headless.step);
}
