#!/usr/bin/env bash
#MISE hide=true
set -euo pipefail

case "$(zig env target)" in
  *-linux*) ;;
  *) exit 0 ;;
esac

source ../../config/pixi-env.sh

glslangValidator -V render/vulkan/shaders/fullscreen.vert -o render/vulkan/shaders/fullscreen.vert.spv
glslangValidator -V render/vulkan/shaders/sample.frag -o render/vulkan/shaders/sample.frag.spv
