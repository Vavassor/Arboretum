#ifndef SHADER_H_
#define SHADER_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "gl_core_3_3.h"
#include "memory.h"

GLuint load_shader_program(const char* vertex_name, const char* fragment_name, Stack* stack);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // SHADER_H_
