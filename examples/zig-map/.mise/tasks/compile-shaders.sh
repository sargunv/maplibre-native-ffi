#!/usr/bin/env bash
#MISE hide=true
set -euo pipefail

case "$(zig env target)" in
  *-linux*) ;;
  *) exit 0 ;;
esac

pixi run glslangValidator -V render/vulkan/shaders/fullscreen.vert -o render/vulkan/shaders/fullscreen.vert.spv
pixi run glslangValidator -V render/vulkan/shaders/sample.frag -o render/vulkan/shaders/sample.frag.spv
