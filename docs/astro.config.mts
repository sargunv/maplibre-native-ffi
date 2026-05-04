// @ts-check

import starlight from "@astrojs/starlight";
import { defineConfig } from "astro/config";

// https://astro.build/config
export default defineConfig({
  integrations: [
    starlight({
      title: "MapLibre Native FFI",
      social: [
        {
          icon: "github",
          label: "GitHub",
          href: "https://github.com/sargunv/maplibre-native-ffi",
        },
      ],
      sidebar: [
        { label: "Overview", link: "/" },
        { label: "Quickstart", slug: "start/quickstart" },
        { label: "Status", slug: "start/status" },
        { label: "Concepts", slug: "concepts" },
        {
          label: "Usage",
          items: [
            {
              label: "Create a Runtime and Map",
              slug: "guides/create-a-runtime-and-map",
            },
            {
              label: "Drive the Event Loop",
              slug: "guides/drive-the-event-loop",
            },
            {
              label: "Render to a Surface",
              slug: "guides/render-to-a-surface",
            },
            {
              label: "Render to a Texture",
              slug: "guides/render-to-a-texture",
            },
            {
              label: "Load Styles and Resources",
              slug: "guides/load-styles-and-resources",
            },
            { label: "Control the Camera", slug: "guides/control-the-camera" },
            { label: "Add Style Data", slug: "guides/add-style-data" },
            { label: "Query Features", slug: "guides/query-features" },
            { label: "Handle Events", slug: "guides/handle-events" },
            {
              label: "Handle Diagnostics",
              slug: "guides/handle-errors-and-diagnostics",
            },
          ],
        },
        {
          label: "Development",
          items: [
            { label: "Overview", slug: "development/overview" },
            { label: "C API", slug: "development/c-api" },
            { label: "C# Bindings", slug: "development/csharp-bindings" },
            {
              label: "Dart (Flutter) Bindings",
              slug: "development/dart-flutter-bindings",
            },
            { label: "Go Bindings", slug: "development/go-bindings" },
            {
              label: "Java (FFM) Bindings",
              slug: "development/java-ffm-bindings",
            },
            {
              label: "Java (JNI) Bindings",
              slug: "development/java-jni-bindings",
            },
            { label: "Kotlin Bindings", slug: "development/kotlin-bindings" },
            { label: "Python Bindings", slug: "development/python-bindings" },
            { label: "Rust Bindings", slug: "development/rust-bindings" },
            { label: "Swift Bindings", slug: "development/swift-bindings" },
            {
              label: "TypeScript (Node) Bindings",
              slug: "development/typescript-node-bindings",
            },
            { label: "Zig Bindings", slug: "development/zig-bindings" },
          ],
        },
        {
          label: "Reference",
          items: [{ label: "C API", slug: "reference/c" }],
        },
      ],
    }),
  ],
});
