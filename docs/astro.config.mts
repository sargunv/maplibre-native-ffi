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
        { label: "Concepts", slug: "concepts" },
        {
          label: "Usage",
          autogenerate: { directory: "guides" },
        },
        {
          label: "Reference",
          items: [{ label: "C API", slug: "reference/c" }],
        },
        {
          label: "Development",
          autogenerate: { directory: "development" },
        },
      ],
    }),
  ],
});
