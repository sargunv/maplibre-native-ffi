---
title: "C API"
description: "Public C ABI reference for MapLibre Native FFI."
---

{{#each filtered.sections}}
{{#if (eq section "func")}}
## Functions

{{#each members}}
{{cleanAnchor refid name}}

### {{name}}

{{badges}}

```c
{{signature}}
```

{{briefdescription}}

{{detaileddescription}}

{{#unless briefdescription}}
{{#unless detaileddescription}}
{{memberSummary this}}
{{/unless}}
{{/unless}}

{{#if (hasDocumentedParams params)}}
| Parameter | Type | Description |
|-----------|------|-------------|
{{#each (documentedParams params)}}| `{{name}}` | `{{type}}` | {{description}} |
{{/each}}
{{/if}}

{{/each}}
{{/if}}
{{/each}}

{{#each filtered.sections}}
{{#if (eq section "enum")}}
## Enumerations

{{#each members}}
{{cleanAnchor refid name}}

### {{name}}

```c
{{signature}}
```

{{briefdescription}}

{{detaileddescription}}

{{#if enumvalue}}
| Value | Description |
|-------|-------------|
{{#each enumvalue}}| `{{name}}` | {{summary}} |
{{/each}}
{{/if}}

{{/each}}
{{/if}}
{{/each}}

{{#each filtered.sections}}
{{#if (eq section "typedef")}}
## Types

{{#each members}}
{{cleanAnchor refid name}}

### {{name}}

```c
{{definition}};
```

{{briefdescription}}

{{detaileddescription}}

{{#unless briefdescription}}
{{#unless detaileddescription}}
{{memberSummary this}}
{{/unless}}
{{/unless}}

{{/each}}
{{/if}}
{{/each}}

{{#each filtered.sections}}
{{#unless (or (or (eq section "func") (eq section "enum")) (eq section "typedef"))}}
## {{label}}

{{#each members}}
{{cleanAnchor refid name}}

### {{name}}

{{badges}}

```c
{{signature}}
```

{{briefdescription}}

{{detaileddescription}}

{{#unless briefdescription}}
{{#unless detaileddescription}}
{{memberSummary this}}
{{/unless}}
{{/unless}}

{{#if enumvalue}}
| Value | Description |
|-------|-------------|
{{#each enumvalue}}| `{{name}}` | {{summary}} |
{{/each}}
{{/if}}

{{/each}}
{{/unless}}
{{/each}}
