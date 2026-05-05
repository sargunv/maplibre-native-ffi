// @ts-check

import starlight from "@astrojs/starlight";
import { defineConfig } from "astro/config";
import starlightCopyButton from "starlight-copy-button";
import starlightLinksValidator from "starlight-links-validator";
import starlightLlmsTxt from "starlight-llms-txt";

// https://astro.build/config
export default defineConfig({
  site: "https://code.sargunv.dev",
  base: "/maplibre-native-ffi",
  integrations: [
    starlight({
      title: "MapLibre Native FFI",
      logo: {
        light: "./src/assets/maplibre-logo-square-for-light-bg.svg",
        dark: "./src/assets/maplibre-logo-square-for-dark-bg.svg",
      },
      editLink: {
        baseUrl:
          "https://github.com/sargunv/maplibre-native-ffi/edit/main/docs/",
      },
      customCss: ["./src/styles/custom.css"],
      plugins: [
        starlightCopyButton(),
        starlightLlmsTxt({ exclude: ["reference/**"] }),
        starlightLinksValidator(),
      ],
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
