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
        {
          label: "Start Here",
          items: [
            { label: "Overview", slug: "start/overview" },
            { label: "Quickstart", slug: "start/quickstart" },
            { label: "Platform Support", slug: "start/platform-support" },
            { label: "Project Status", slug: "start/project-status" },
          ],
        },
        {
          label: "Guides",
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
          label: "Concepts",
          items: [{ label: "Concepts", slug: "concepts" }],
        },
        {
          label: "Development",
          items: [
            { label: "Setup", slug: "development/setup" },
            { label: "Guidelines", slug: "development/guidelines" },
            { label: "C", slug: "development/c" },
            { label: "Swift", slug: "development/swift" },
            { label: "Kotlin", slug: "development/kotlin" },
            { label: "Java", slug: "development/java" },
            { label: "Zig", slug: "development/zig" },
          ],
        },
        {
          label: "Reference",
          items: [
            { label: "C", slug: "reference/c" },
            { label: "Swift", slug: "reference/swift" },
            { label: "Kotlin", slug: "reference/kotlin" },
            { label: "Java", slug: "reference/java" },
            { label: "Zig", slug: "reference/zig" },
          ],
        },
      ],
    }),
  ],
});
