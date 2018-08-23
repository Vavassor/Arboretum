# Arboretum

Arboretum is a 3D modeling app for desktop Windows and Linux. It focuses on
modeling, painting, and animating models for games and art.

It is currently a prototype and won't be released anywhere until further notice.

## Building

This project uses [CMake](https://cmake.org). CMake files can be used directly
by many editors, or can generate files for other build systems, such as Unix
Makefiles. For now, consult a guide for your editor or build system (more
useful explanation coming soon!).

### Shaders

Shaders are written in HLSL and are translated to GLSL by
[XShaderCompiler](https://github.com/LukasBanana/XShaderCompiler). A python
script [build_shader.py](Tools/build_shaders.py) is used to translate all of
them at once. It's not integrated with the CMake build; it's run separately.

### Unicode Property Tables

There's also a script for building tables to look up Unicode properties called
[MultiStageTable.py](Tools/MultiStageTable.py). The tables should not be rebuilt
except if updating to a new Unicode version. This would also require changes to
the algorithms using these properties, so running updated tables through the
outdated code would cause malfunction.

