// @ts-check

import starlight from "@astrojs/starlight";
import { defineConfig } from "astro/config";
import gruvbox from "starlight-theme-gruvbox";

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
      plugins: [gruvbox()],
      sidebar: [
        {
          label: "Guides",
          items: [
            // Each item here is one entry in the navigation menu.
            { label: "Example Guide", slug: "guides/example" },
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
