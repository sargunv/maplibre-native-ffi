const std = @import("std");

const BuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
};

fn linkMapLibreAbi(b: *std.Build, module: *std.Build.Module) void {
    module.addIncludePath(b.path("include"));
    module.addLibraryPath(b.path("build"));
    module.addRPath(b.path("build"));
    module.linkSystemLibrary("maplibre_native_abi", .{});
    module.link_libc = true;
}

fn addHeadlessExample(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const headless = b.addExecutable(.{
        .name = "headless_zig_example",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/zig-headless/main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreAbi(b, headless.root_module);
    b.installArtifact(headless);
    return headless;
}

fn addAbiTests(b: *std.Build, options: BuildOptions) *std.Build.Step.Compile {
    const abi_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests/abi/main.zig"),
            .target = options.target,
            .optimize = options.optimize,
        }),
    });

    linkMapLibreAbi(b, abi_tests.root_module);
    return abi_tests;
}

pub fn build(b: *std.Build) void {
    const options = BuildOptions{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
    };

    const headless = addHeadlessExample(b, options);
    const abi_tests = addAbiTests(b, options);

    const run_headless = b.addRunArtifact(headless);
    const run_step = b.step("run", "Run headless Zig example");
    run_step.dependOn(&run_headless.step);

    const run_abi_tests = b.addRunArtifact(abi_tests);
    const test_step = b.step("test", "Run Zig ABI tests");
    test_step.dependOn(&run_abi_tests.step);
}
