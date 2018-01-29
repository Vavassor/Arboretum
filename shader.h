#ifndef SHADER_H_
#define SHADER_H_

#include "gl_core_3_3.h"

struct Stack;
GLuint load_shader_program(const char* vertex_source, const char* fragment_source, Stack* stack);

#endif // SHADER_H_
