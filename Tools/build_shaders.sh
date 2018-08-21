#!/bin/bash

# The "xsc" in this file is https://github.com/LukasBanana/XShaderCompiler .
# It will need to be installed for this script to work correctly.
# To use, navigate to Assets/Shaders and run:
#   ../../Tools/build_shaders.sh ./

for value in $1/*.vs.hlsl
do
no_extension=$(basename "$value" .vs.hlsl)
name="$no_extension.vs.glsl"
xsc -EB -E vertex_main -o "$name" -T vert "$value"
done

for value in $1/*.fs.hlsl
do
no_extension=$(basename "$value" .fs.hlsl)
name="$no_extension.fs.glsl"
xsc -EB -E pixel_main -o "$name" -T frag "$value"
done

