#include "immediate.h"

#include "gl_core_3_3.h"
#include "assert.h"
#include "vertex_layout.h"
#include "memory.h"

namespace immediate {

enum class DrawMode
{
	None,
	Lines,
	Triangles,
};

enum class VertexType
{
	None,
	Colour,
	Texture,
};

struct Context;

namespace
{
	const int context_vertices_cap = 8192;
	const int context_vertex_type_count = 2;
	Context* context;
}

struct Context
{
	union
	{
		VertexPC vertices[context_vertices_cap];
		VertexPT vertices_textured[context_vertices_cap];
	};
	Matrix4 view_projection;
	GLuint vertex_arrays[context_vertex_type_count];
	GLuint buffers[context_vertex_type_count];
	GLuint shaders[context_vertex_type_count];
	int filled;
	DrawMode draw_mode;
	BlendMode blend_mode;
	VertexType vertex_type;
	bool blend_mode_changed;
};

void context_create(Heap* heap)
{
	context = HEAP_ALLOCATE(heap, Context, 1);
	Context* c = context;

	glGenVertexArrays(context_vertex_type_count, c->vertex_arrays);
	glGenBuffers(context_vertex_type_count, c->buffers);

	glBindVertexArray(c->vertex_arrays[0]);
	glBindBuffer(GL_ARRAY_BUFFER, c->buffers[0]);

	glBufferData(GL_ARRAY_BUFFER, sizeof(c->vertices), nullptr, GL_DYNAMIC_DRAW);

	GLvoid* offset0 = reinterpret_cast<GLvoid*>(offsetof(VertexPC, position));
	GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPC, colour));
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPC), offset0);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexPC), offset1);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(2);

	glBindVertexArray(c->vertex_arrays[1]);
	glBindBuffer(GL_ARRAY_BUFFER, c->buffers[1]);

	glBufferData(GL_ARRAY_BUFFER, sizeof(c->vertices_textured), nullptr, GL_DYNAMIC_DRAW);

	offset0 = reinterpret_cast<GLvoid*>(offsetof(VertexPT, position));
	offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPT, texcoord));
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPT), offset0);
	glVertexAttribPointer(3, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(VertexPT), offset1);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(3);

	glBindVertexArray(0);
}

void context_destroy(Heap* heap)
{
	if(context)
	{
		glDeleteBuffers(context_vertex_type_count, context->buffers);
		glDeleteVertexArrays(context_vertex_type_count, context->vertex_arrays);
		HEAP_DEALLOCATE(heap, context);
	}
}

void set_matrices(Matrix4 view, Matrix4 projection)
{
	context->view_projection = projection * view;
}

void set_shader(GLuint program)
{
	context->shaders[0] = program;
}

void set_textured_shader(GLuint program)
{
	context->shaders[1] = program;
}

void set_blend_mode(BlendMode mode)
{
	if(context->blend_mode != mode)
	{
		context->blend_mode = mode;
		context->blend_mode_changed = true;
	}
}

void set_clip_area(Rect rect, int viewport_width, int viewport_height)
{
	glEnable(GL_SCISSOR_TEST);
	int x = rect.bottom_left.x + (viewport_width / 2);
	int y = rect.bottom_left.y + (viewport_height / 2);
	glScissor(x, y, rect.dimensions.x, rect.dimensions.y);
}

void stop_clip_area()
{
	glDisable(GL_SCISSOR_TEST);
}

static GLenum get_mode(DrawMode draw_mode)
{
	switch(draw_mode)
	{
		default:
		case DrawMode::Lines:     return GL_LINES;
		case DrawMode::Triangles: return GL_TRIANGLES;
	}
}

void draw()
{
	Context* c = context;
	if(c->filled == 0 || c->draw_mode == DrawMode::None || c->vertex_type == VertexType::None)
	{
		return;
	}

	if(c->blend_mode_changed)
	{
		switch(c->blend_mode)
		{
			case BlendMode::None:
			case BlendMode::Opaque:
			{
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				glEnable(GL_CULL_FACE);
				break;
			}
			case BlendMode::Transparent:
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDepthMask(GL_FALSE);
				glDisable(GL_CULL_FACE);
				break;
			}
		}
		c->blend_mode_changed = false;
	}

	GLuint shader;
	switch(c->vertex_type)
	{
		case VertexType::None:
		case VertexType::Colour:
		{
			glBindBuffer(GL_ARRAY_BUFFER, c->buffers[0]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPC) * c->filled, c->vertices, GL_DYNAMIC_DRAW);
			glBindVertexArray(c->vertex_arrays[0]);
			shader = c->shaders[0];
			break;
		}
		case VertexType::Texture:
		{
			glBindBuffer(GL_ARRAY_BUFFER, c->buffers[1]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPT) * c->filled, c->vertices_textured, GL_DYNAMIC_DRAW);
			glBindVertexArray(c->vertex_arrays[1]);
			shader = c->shaders[1];
			break;
		}
	}

	glUseProgram(shader);
	GLint location = glGetUniformLocation(shader, "model_view_projection");
	glUniformMatrix4fv(location, 1, GL_TRUE, c->view_projection.elements);

	glDrawArrays(get_mode(c->draw_mode), 0, c->filled);

	c->draw_mode = DrawMode::None;
	set_blend_mode(BlendMode::None);
	c->vertex_type = VertexType::None;
	c->filled = 0;
}

void add_line(Vector3 start, Vector3 end, Vector4 colour)
{
	Context* c = context;
	ASSERT(c->draw_mode == DrawMode::Lines || c->draw_mode == DrawMode::None);
	ASSERT(c->vertex_type == VertexType::Colour || c->vertex_type == VertexType::None);
	ASSERT(c->filled + 2 < context_vertices_cap);
	u32 colour_u32 = rgba_to_u32(colour);
	c->vertices[c->filled + 0] = {start, colour_u32};
	c->vertices[c->filled + 1] = {end, colour_u32};
	c->filled += 2;
	c->draw_mode = DrawMode::Lines;
	c->vertex_type = VertexType::Colour;
}

void add_rect(Rect rect, Vector4 colour)
{
	Quad quad = rect_to_quad(rect);
	add_quad(&quad, colour);
}

void add_wire_rect(Rect rect, Vector4 colour)
{
	Quad quad = rect_to_quad(rect);
	add_wire_quad(&quad, colour);
}

void add_quad(Quad* quad, Vector4 colour)
{
	Context* c = context;
	ASSERT(c->draw_mode == DrawMode::Triangles || c->draw_mode == DrawMode::None);
	ASSERT(c->vertex_type == VertexType::Colour || c->vertex_type == VertexType::None);
	ASSERT(c->filled + 6 < context_vertices_cap);
	c->vertices[c->filled + 0].position = quad->vertices[0];
	c->vertices[c->filled + 1].position = quad->vertices[1];
	c->vertices[c->filled + 2].position = quad->vertices[2];
	c->vertices[c->filled + 3].position = quad->vertices[0];
	c->vertices[c->filled + 4].position = quad->vertices[2];
	c->vertices[c->filled + 5].position = quad->vertices[3];
	for(int i = 0; i < 6; ++i)
	{
		c->vertices[c->filled + i].colour = rgba_to_u32(colour);
	}
	c->filled += 6;
	c->draw_mode = DrawMode::Triangles;
	c->vertex_type = VertexType::Colour;
}

void add_quad_textured(Quad* quad, Rect texture_rect)
{
	Context* c = context;
	ASSERT(c->draw_mode == DrawMode::Triangles || c->draw_mode == DrawMode::None);
	ASSERT(c->vertex_type == VertexType::Texture || c->vertex_type == VertexType::None);
	ASSERT(c->filled + 6 < context_vertices_cap);
	c->vertices_textured[c->filled + 0].position = quad->vertices[0];
	c->vertices_textured[c->filled + 1].position = quad->vertices[1];
	c->vertices_textured[c->filled + 2].position = quad->vertices[2];
	c->vertices_textured[c->filled + 3].position = quad->vertices[0];
	c->vertices_textured[c->filled + 4].position = quad->vertices[2];
	c->vertices_textured[c->filled + 5].position = quad->vertices[3];
	const Vector2 texcoords[4] =
	{
		{texture_rect.bottom_left.x, texture_rect.bottom_left.y + texture_rect.dimensions.y},
		texture_rect.bottom_left + texture_rect.dimensions,
		{texture_rect.bottom_left.x + texture_rect.dimensions.x, texture_rect.bottom_left.y},
		texture_rect.bottom_left,
	};
	c->vertices_textured[c->filled + 0].texcoord = texcoord_to_u32(texcoords[0]);
	c->vertices_textured[c->filled + 1].texcoord = texcoord_to_u32(texcoords[1]);
	c->vertices_textured[c->filled + 2].texcoord = texcoord_to_u32(texcoords[2]);
	c->vertices_textured[c->filled + 3].texcoord = texcoord_to_u32(texcoords[0]);
	c->vertices_textured[c->filled + 4].texcoord = texcoord_to_u32(texcoords[2]);
	c->vertices_textured[c->filled + 5].texcoord = texcoord_to_u32(texcoords[3]);
	c->filled += 6;
	c->draw_mode = DrawMode::Triangles;
	c->vertex_type = VertexType::Texture;
}

void add_wire_quad(Quad* quad, Vector4 colour)
{
	add_line(quad->vertices[0], quad->vertices[1], colour);
	add_line(quad->vertices[1], quad->vertices[2], colour);
	add_line(quad->vertices[2], quad->vertices[3], colour);
	add_line(quad->vertices[3], quad->vertices[0], colour);
}

void draw_opaque_rect(Rect rect, Vector4 colour)
{
	add_rect(rect, colour);
	draw();
}

void draw_transparent_rect(Rect rect, Vector4 colour)
{
	set_blend_mode(immediate::BlendMode::Transparent);
	add_rect(rect, colour);
	draw();
}

} // namespace immediate
