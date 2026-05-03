import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  integrations: [
    starlight({
      title: 'MapLibre Native FFI',
      sidebar: [
        {
          label: 'Getting Started',
          items: [
            { label: 'Welcome', link: '/getting-started/welcome' },
            { label: 'Choose a Package', link: '/getting-started/choose-a-package' },
            { label: 'Choose a Variant', link: '/getting-started/choose-a-variant' },
            { label: 'Build from Source', link: '/getting-started/build-from-source' },
            { label: 'First Map Checklist', link: '/getting-started/first-map-checklist' }
          ]
        },
        {
          label: 'Usage',
          items: [
            { label: 'Runtime, Map, and Sessions', link: '/usage/runtime-map-sessions' },
            { label: 'Event Polling Model', link: '/usage/event-polling' },
            { label: 'Programmatic Styling', link: '/usage/programmatic-styling' },
            { label: 'Render Sessions', link: '/usage/render-sessions' },
            { label: 'Custom Geometry Sources', link: '/usage/custom-geometry-sources' },
            { label: 'Offline Manager', link: '/usage/offline-manager' }
          ]
        },
        {
          label: 'API References',
          items: [
            { label: 'Overview', link: '/api-references/overview' },
            { label: 'C API (Doxygen)', link: '/generated/api/c/index.html' }
          ]
        },
        {
          label: 'Development',
          items: [
            { label: 'Development Home', link: '/development/index' },
            { label: 'Project Scope', link: '/development/project-scope' },
            { label: 'C ABI & API Design', link: '/development/c-abi-and-api-design' },
            { label: 'Ownership & Lifetimes', link: '/development/ownership-and-lifetimes' },
            { label: 'Threading, Async, and Events', link: '/development/threading-async-events' },
            { label: 'Testing and Examples', link: '/development/testing-and-examples' },
            { label: 'Binding Guidelines', link: '/development/binding-guidelines' }
          ]
        }
      ]
    })
  ]
});
