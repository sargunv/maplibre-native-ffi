const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const hello = b.addExecutable(.{
        .name = "hello_zig_example",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/zig-hello/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    hello.root_module.addIncludePath(b.path("include"));
    hello.root_module.addLibraryPath(b.path("build"));
    hello.root_module.addRPath(b.path("build"));
    hello.root_module.linkSystemLibrary("maplibre_native_abi", .{});
    hello.root_module.link_libc = true;

    b.installArtifact(hello);

    const run_hello = b.addRunArtifact(hello);
    const run_step = b.step("run", "Run hello-world Zig example");
    run_step.dependOn(&run_hello.step);
}
