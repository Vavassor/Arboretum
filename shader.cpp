#include "shader.h"

#include "logging.h"
#include "memory.h"
#include "string_utilities.h"

GLuint load_shader(GLenum type, const char* source, Stack* stack)
{
	GLuint shader = glCreateShader(type);
	GLint source_size = string_size(source);
	glShaderSource(shader, 1, &source, &source_size);
	glCompileShader(shader);

	// Output any errors if the compilation failed.

	GLint compile_status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if(compile_status == GL_FALSE)
	{
		GLint info_log_size = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_size);
		if(info_log_size > 0)
		{
			GLchar* info_log = STACK_ALLOCATE(stack, GLchar, info_log_size);
			if(info_log)
			{
				GLsizei bytes_written = 0;
				glGetShaderInfoLog(shader, info_log_size, &bytes_written, info_log);
				LOG_ERROR("Couldn't compile the shader.\n%s", info_log);
				STACK_DEALLOCATE(stack, info_log);
			}
			else
			{
				LOG_ERROR("Couldn't compile the shader.");
			}
		}

		glDeleteShader(shader);

		return 0;
	}

	return shader;
}

GLuint load_shader_program(const char* vertex_source, const char* fragment_source, Stack* stack)
{
	GLuint program;

	GLuint vertex_shader = load_shader(GL_VERTEX_SHADER, vertex_source, stack);
	if(vertex_shader == 0)
	{
		LOG_ERROR("Failed to load the vertex shader.");
		return 0;
	}

	GLuint fragment_shader = load_shader(GL_FRAGMENT_SHADER, fragment_source, stack);
	if(fragment_shader == 0)
	{
		LOG_ERROR("Failed to load the fragment shader.");
		glDeleteShader(vertex_shader);
		return 0;
	}

	// Create the program object and link the shaders to it.

	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	// Check if linking failed and output any errors.

	GLint link_status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if(link_status == GL_FALSE)
	{
		int info_log_size = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_size);
		if(info_log_size > 0)
		{
			GLchar* info_log = STACK_ALLOCATE(stack, GLchar, info_log_size);
			if(info_log)
			{
				GLsizei bytes_written = 0;
				glGetProgramInfoLog(program, info_log_size, &bytes_written, info_log);
				LOG_ERROR("Couldn't link the shader program.\n%s", info_log);
				STACK_DEALLOCATE(stack, info_log);
			}
			else
			{
				LOG_ERROR("Couldn't link the shader program.");
			}
		}

		glDeleteProgram(program);
		glDeleteShader(fragment_shader);
		glDeleteShader(vertex_shader);

		return 0;
	}

	// Shaders are no longer needed after the program object is linked.
	glDetachShader(program, vertex_shader);
	glDetachShader(program, fragment_shader);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}
