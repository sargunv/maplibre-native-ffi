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
          label: "Usage",
          items: [{ label: "Overview", slug: "usage" }],
        },
        {
          label: "Development",
          items: [
            { label: "Setup", slug: "development/setup" },
            { label: "Conventions", slug: "development/conventions" },
          ],
        },
        {
          label: "Reference",
          items: [{ label: "C API", slug: "api/c" }],
        },
      ],
    }),
  ],
});
