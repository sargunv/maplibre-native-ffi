// @ts-check

import starlight from "@astrojs/starlight";
import { defineConfig } from "astro/config";

// https://astro.build/config
export default defineConfig({
  vite: {
    plugins: [
      {
        name: "serve-public-directory-indexes",
        configureServer(server) {
          server.middlewares.use((request, _response, next) => {
            if (request.url === "/api/c/") request.url = "/api/c/index.html";
            next();
          });
        },
      },
    ],
  },
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
          label: "Guides",
          items: [
            // Each item here is one entry in the navigation menu.
            { label: "Example Guide", slug: "guides/example" },
          ],
        },
        {
          label: "Reference",
          items: [{ label: "C API", link: "/api/c/" }],
        },
      ],
    }),
  ],
});
