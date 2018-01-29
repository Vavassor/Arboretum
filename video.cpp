#include "video.h"

#include "float_utilities.h"
#include "int_utilities.h"
#include "string_utilities.h"
#include "string_build.h"
#include "vector_math.h"
#include "gl_core_3_3.h"
#include "vertex_layout.h"
#include "shader.h"
#include "jan.h"
#include "intersection.h"
#include "input.h"
#include "logging.h"
#include "memory.h"
#include "assert.h"
#include "array.h"
#include "bmp.h"
#include "obj.h"
#include "immediate.h"
#include "math_basics.h"
#include "bmfont.h"
#include "ui.h"
#include "filesystem.h"
#include "sorting.h"
#include "loop_macros.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace video {

const char* lit_vertex_source = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;
uniform mat4x4 normal_matrix;

out vec3 surface_normal;
out vec3 surface_colour;

void main()
{
	gl_Position = model_view_projection * vec4(position, 1.0);
	surface_normal = (normal_matrix * vec4(normal, 0.0)).xyz;
	surface_colour = colour.rgb;
}
)";

const char* lit_fragment_source = R"(
#version 330

layout(location = 0) out vec4 output_colour;

uniform vec3 light_direction;

in vec3 surface_normal;
in vec3 surface_colour;

float half_lambert(vec3 n, vec3 l)
{
	return 0.5 * dot(n, l) + 0.5;
}

float lambert(vec3 n, vec3 l)
{
	return max(dot(n, l), 0.0);
}

void main()
{
	float light = half_lambert(surface_normal, light_direction);
	output_colour = vec4(surface_colour * vec3(light), 1.0);
}
)";

const char* vertex_source_vertex_colour = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;

out vec4 surface_colour;

void main()
{
	gl_Position = model_view_projection * vec4(position, 1.0);
	surface_colour = colour;
}
)";

const char* fragment_source_vertex_colour = R"(
#version 330

layout(location = 0) out vec4 output_colour;

in vec4 surface_colour;

void main()
{
	output_colour = surface_colour;
}
)";

const char* vertex_source_texture_only = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;

uniform mat4x4 model_view_projection;

out vec2 surface_texcoord;

void main()
{
	gl_Position = model_view_projection * vec4(position, 1.0);
	surface_texcoord = texcoord;
}
)";

const char* fragment_source_texture_only = R"(
#version 330

uniform sampler2D texture;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
	output_colour = vec4(texture2D(texture, surface_texcoord).rgb, 1.0);
}
)";


const char* vertex_source_font = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;

uniform mat4x4 model_view_projection;

out vec2 surface_texcoord;

void main()
{
	gl_Position = model_view_projection * vec4(position, 1.0);
	surface_texcoord = texcoord;
}
)";

const char* fragment_source_font = R"(
#version 330

uniform sampler2D texture;
uniform vec3 text_colour;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
	vec4 colour = texture2D(texture, surface_texcoord);
	colour.rgb *= text_colour;
	output_colour = colour;
}
)";

const char* vertex_source_halo = R"(
#version 330

layout(location = 0) in vec3 position;

uniform mat4x4 model_view_projection;

void main()
{
	gl_Position = model_view_projection * vec4(position, 1.0);
}
)";

const char* fragment_source_halo = R"(
#version 330

layout(location = 0) out vec4 output_colour;

void main()
{
	output_colour = vec4(1.0);
}
)";

struct Object
{
	Matrix4 model_view_projection;
	Matrix4 normal;
	GLuint buffers[2];
	GLuint vertex_array;
	int indices_count;
};

static void object_create(Object* object)
{
	glGenVertexArrays(1, &object->vertex_array);
	glGenBuffers(2, object->buffers);
}

static void object_destroy(Object* object)
{
	glDeleteVertexArrays(1, &object->vertex_array);
	glDeleteBuffers(2, object->buffers);
}

static void object_set_surface(Object* object, VertexPNC* vertices, int vertices_count, u16* indices, int indices_count)
{
	glBindVertexArray(object->vertex_array);

	const int vertex_size = sizeof(VertexPNC);
	GLsizei vertices_size = vertex_size * vertices_count;
	GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPNC, normal));
	GLvoid* offset2 = reinterpret_cast<GLvoid*>(offsetof(VertexPNC, colour));
	glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, nullptr);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, offset1);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, offset2);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	GLsizei indices_size = sizeof(u16) * indices_count;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);
	object->indices_count = indices_count;

	glBindVertexArray(0);
}

static void object_update_mesh(Object* object, jan::Mesh* mesh, Heap* heap)
{
	VertexPNC* vertices;
	int vertices_count;
	u16* indices;
	int indices_count;
	jan::triangulate(mesh, heap, &vertices, &vertices_count, &indices, &indices_count);

	glBindVertexArray(object->vertex_array);

	const int vertex_size = sizeof(VertexPNC);
	GLsizei vertices_size = vertex_size * vertices_count;
	glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_DYNAMIC_DRAW);

	GLsizei indices_size = sizeof(u16) * indices_count;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_DYNAMIC_DRAW);

	glBindVertexArray(0);

	HEAP_DEALLOCATE(heap, vertices);
	HEAP_DEALLOCATE(heap, indices);
}

static void object_set_matrices(Object* object, Matrix4 model, Matrix4 view, Matrix4 projection)
{
	Matrix4 model_view = view * model;
	object->model_view_projection = projection * model_view;
	object->normal = transpose(inverse_transform(model_view));
}

void object_generate_sky(Object* object, Stack* stack)
{
	const float radius = 1.0f;
	const int meridians = 9;
	const int parallels = 7;
	int rings = parallels + 1;

	int vertices_count = meridians * parallels + 2;
	VertexPNC* vertices = STACK_ALLOCATE(stack, VertexPNC, vertices_count);
	if(!vertices)
	{
		return;
	}

	const Vector3 top_colour = {1.0f, 1.0f, 0.2f};
	const Vector3 bottom_colour = {0.1f, 0.7f, 0.6f};
	vertices[0].position = radius * vector3_unit_z;
	vertices[0].normal = -vector3_unit_z;
	vertices[0].colour = rgb_to_u32(top_colour);
	for(int i = 0; i < parallels; ++i)
	{
		float step = (i + 1) / static_cast<float>(rings);
		float theta = step * pi;
		Vector3 ring_colour = lerp(top_colour, bottom_colour, step);
		for(int j = 0; j < meridians; ++j)
		{
			float phi = (j + 1) / static_cast<float>(meridians) * tau;
			float x = radius * sin(theta) * cos(phi);
			float y = radius * sin(theta) * sin(phi);
			float z = radius * cos(theta);
			Vector3 position = {x, y, z};
			VertexPNC* vertex = &vertices[meridians * i + j + 1];
			vertex->position = position;
			vertex->normal = -normalise(position);
			vertex->colour = rgb_to_u32(ring_colour);
		}
	}
	vertices[vertices_count - 1].position = radius * -vector3_unit_z;
	vertices[vertices_count - 1].normal = vector3_unit_z;
	vertices[vertices_count - 1].colour = rgb_to_u32(bottom_colour);

	int indices_count = 6 * meridians * rings;
	u16* indices = STACK_ALLOCATE(stack, u16, indices_count);
	if(!indices)
	{
		STACK_DEALLOCATE(stack, vertices);
		return;
	}

	int out_base = 0;
	int in_base = 1;
	for(int i = 0; i < meridians; ++i)
	{
		int o = out_base + 3 * i;
		indices[o + 0] = 0;
		indices[o + 1] = in_base + (i + 1) % meridians;
		indices[o + 2] = in_base + i;
	}
	out_base += 3 * meridians;
	for(int i = 0; i < rings - 2; ++i)
	{
		for(int j = 0; j < meridians; ++j)
		{
			int x = meridians * i + j;
			int o = out_base + 6 * x;
			int k0 = in_base + x;
			int k1 = in_base + meridians * i;

			indices[o + 0] = k0;
			indices[o + 1] = k1 + (j + 1) % meridians;
			indices[o + 2] = k0 + meridians;

			indices[o + 3] = k0 + meridians;
			indices[o + 4] = k1 + (j + 1) % meridians;
			indices[o + 5] = k1 + meridians + (j + 1) % meridians;
		}
	}
	out_base += 6 * meridians * (rings - 2);
	in_base += meridians * (parallels - 2);
	for(int i = 0; i < meridians; ++i)
	{
		int o = out_base + 3 * i;
		indices[o + 0] = vertices_count - 1;
		indices[o + 1] = in_base + i;
		indices[o + 2] = in_base + (i + 1) % meridians;
	}

	object_set_surface(object, vertices, vertices_count, indices, indices_count);

	STACK_DEALLOCATE(stack, vertices);
	STACK_DEALLOCATE(stack, indices);
}

struct Pixel8
{
	u8 r;
};

struct Pixel24
{
	u8 r, g, b;
};

struct Pixel32
{
	u8 a, r, g, b;
};

struct Bitmap
{
	void* pixels;
	int width;
	int height;
	int bytes_per_pixel;
};

GLuint upload_bitmap(Bitmap* bitmap)
{
	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	switch(bitmap->bytes_per_pixel)
	{
		case 1:
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, bitmap->width, bitmap->height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap->pixels);
			break;
		}
		case 2:
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, bitmap->width, bitmap->height, 0, GL_RG, GL_UNSIGNED_BYTE, bitmap->pixels);
			break;
		}
		case 3:
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, bitmap->width, bitmap->height, 0, GL_RGB, GL_UNSIGNED_BYTE, bitmap->pixels);
			break;
		}
		case 4:
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bitmap->width, bitmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels);
			break;
		}
	}

	return texture;
}

static void draw_text(const char* text, Vector2 baseline_start, bmfont::Font* font)
{
	Vector2 texture_dimensions;
	texture_dimensions.x = font->image_width;
	texture_dimensions.y = font->image_height;

	Vector2 pen = baseline_start;
	char32_t prior_char = '\0';

	int size = string_size(text);
	for(int i = 0; i < size; i += 1)
	{
		char32_t current = text[i];
		bmfont::Glyph* glyph = bmfont::find_glyph(font, current);

		float kerning = bmfont::lookup_kerning(font, prior_char, current);
		pen.x += kerning;

		Vector2 top_left = pen;
		top_left.x += glyph->offset.x;
		top_left.y -= glyph->offset.y;

		Rect viewport_rect;
		viewport_rect.dimensions = glyph->rect.dimensions;
		viewport_rect.bottom_left.x = top_left.x;
		viewport_rect.bottom_left.y = top_left.y + font->line_height - viewport_rect.dimensions.y;
		Quad quad = rect_to_quad(viewport_rect);

		Rect texture_rect = glyph->rect;
		texture_rect.bottom_left = pointwise_divide(texture_rect.bottom_left, texture_dimensions);
		texture_rect.dimensions = pointwise_divide(texture_rect.dimensions, texture_dimensions);

		immediate::add_quad_textured(&quad, texture_rect);

		pen.x += glyph->x_advance;
		prior_char = current;
	}
	immediate::set_blend_mode(immediate::BlendMode::Transparent);
	immediate::draw();
}

static Vector2 compute_text_bounds(const char* text, bmfont::Font* font)
{
	Vector2 bounds;
	bounds.x = 0.0f;
	bounds.y = font->line_height;

	char32_t prior_char = '\0';
	int size = string_size(text);
	for(int i = 0; i < size; i += 1)
	{
		char32_t current = text[i];
		bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
		float kerning = bmfont::lookup_kerning(font, prior_char, current);
		bounds.x += kerning + glyph->x_advance;
		prior_char = current;
	}

	return bounds;
}

struct PathButton
{
	Rect bounds;
	char* text;
};

struct Spacing
{
	float start;
	float top;
	float end;
	float bottom;
};

struct Item
{
	Rect bounds;
	Spacing padding;
};

struct Button
{
	Item item;
	const char* label;
	bool enabled;
	bool hovered;
};

static const int invalid_index = -1;

struct FilePickDialog
{
	Button pick;
	Rect bounds;
	Spacing path_button_padding;
	Spacing path_button_margin;
	Spacing record_padding;
	Spacing record_margin;
	Spacing footer_padding;
	Directory directory;
	char* path;
	Rect* records_bounds;
	PathButton* path_buttons;
	int path_buttons_count;
	int path_button_selected;
	int record_hovered;
	int record_selected;
	float scroll_top;
	bool show_hidden_records;
	bool enabled;
};

static float get_record_height(FilePickDialog* dialog, bmfont::Font* font)
{
	Spacing margin = dialog->record_margin;
	Spacing padding = dialog->record_padding;
	return margin.top + padding.top + font->line_height + padding.bottom + margin.bottom;
}

static float get_path_bar_height(FilePickDialog* dialog, bmfont::Font* font)
{
	Spacing margin = dialog->path_button_margin;
	Spacing padding = dialog->path_button_padding;
	return margin.top + padding.top + font->line_height + padding.bottom + margin.bottom;
}

static float get_footer_height(FilePickDialog* dialog, bmfont::Font* font)
{
	Spacing padding = dialog->footer_padding;
	return padding.top + font->line_height + padding.bottom;
}

static float get_inner_height(FilePickDialog* dialog, bmfont::Font* font)
{
	float path_bar_height = get_path_bar_height(dialog, font);
	float footer_height = get_footer_height(dialog, font);
	return dialog->bounds.dimensions.y - path_bar_height - footer_height;
}

static void add_path_button(FilePickDialog* dialog, const char* path, int end, Vector2 pen, bmfont::Font* font, Heap* heap)
{
	dialog->path_buttons = HEAP_REALLOCATE(heap, PathButton, dialog->path_buttons, dialog->path_buttons_count + 1);
	PathButton* button = &dialog->path_buttons[dialog->path_buttons_count];
	button->text = copy_string_to_heap(path, end, heap);
	button->bounds.bottom_left = pen;
	button->bounds.dimensions = compute_text_bounds(button->text, font);
	dialog->path_buttons_count += 1;
}

static bool record_is_before(DirectoryRecord a, DirectoryRecord b)
{
	if(a.type == DirectoryRecordType::Directory && b.type == DirectoryRecordType::File)
	{
		return true;
	}
	if(a.type == DirectoryRecordType::File && b.type == DirectoryRecordType::Directory)
	{
		return false;
	}
	return compare_alphabetic_ascii(a.name, b.name) < 0;
}

DEFINE_QUICK_SORT(DirectoryRecord, record_is_before, by_filename);

DEFINE_ARRAY(DirectoryRecord);

static void filter_directory(Directory* directory, const char** extensions, int extensions_count, bool show_hidden, Heap* heap)
{
	ArrayDirectoryRecord filtered;
	create(&filtered, heap);

	FOR_N(i, directory->records_count)
	{
		DirectoryRecord record = directory->records[i];
		if(!show_hidden && record.hidden)
		{
			continue;
		}
		if(record.type == DirectoryRecordType::File)
		{
			FOR_N(j, extensions_count)
			{
				if(string_ends_with(record.name, extensions[j]))
				{
					add_and_expand(&filtered, record);
					break;
				}
			}
		}
		else
		{
			add_and_expand(&filtered, record);
		}
	}

	HEAP_DEALLOCATE(heap, directory->records);

	directory->records = filtered.items;
	directory->records_count = filtered.count;
}

static void open_directory(FilePickDialog* dialog, const char* directory, bmfont::Font* font, Heap* heap)
{
	dialog->enabled = true;

	dialog->path = copy_string_onto_heap(directory, heap);

	// Add and layout path buttons at the top of the dialog.
	Spacing margin = dialog->path_button_margin;
	Spacing padding = dialog->path_button_padding;
	float path_bar_height = margin.top + padding.top + font->line_height + padding.bottom + margin.bottom;
	Vector2 top_left = rect_top_left(dialog->bounds);
	Vector2 pen;
	pen.x = top_left.x + margin.start;
	pen.y = top_left.y - font->line_height - margin.top;
	for(const char* path = dialog->path; *path;)
	{
		int found_index = find_char(path, '/');
		if(found_index == 0)
		{
			// root directory
			const char* name = "Filesystem";
			add_path_button(dialog, name, string_size(name), pen, font, heap);
		}
		else if(found_index == -1)
		{
			// final path segment
			add_path_button(dialog, path, string_size(path), pen, font, heap);
			break;
		}
		else
		{
			add_path_button(dialog, path, found_index, pen, font, heap);
		}
		PathButton added = dialog->path_buttons[dialog->path_buttons_count - 1];
		pen.x += padding.start + added.bounds.dimensions.x + padding.end + margin.end + margin.start;
		path += found_index + 1;
	}

	bool listed = list_files_in_directory(dialog->path, &dialog->directory, heap);
	ASSERT(listed);

	const int extensions_count = 1;
	const char* extensions[extensions_count] = {".obj"};
	filter_directory(&dialog->directory, extensions, extensions_count, dialog->show_hidden_records, heap);

	if(dialog->directory.records_count == 0)
	{
		// Listing the directory succeeded, it's just empty.
		dialog->records_bounds = nullptr;
	}
	else
	{
		quick_sort_by_filename(dialog->directory.records, dialog->directory.records_count);

		dialog->records_bounds = HEAP_ALLOCATE(heap, Rect, dialog->directory.records_count);

		margin = dialog->record_margin;
		padding = dialog->record_padding;
		Vector2 dimensions;
		dimensions.x = dialog->bounds.dimensions.x - margin.start - margin.end;
		dimensions.y = padding.top + font->line_height + padding.bottom;
		float advance = margin.top + dimensions.y + margin.bottom;

		Vector2 area_top_left = top_left;
		area_top_left.y -= path_bar_height;

		pen.x = area_top_left.x + margin.start;
		pen.y = area_top_left.y - font->line_height - margin.top;
		FOR_N(i, dialog->directory.records_count)
		{
			Rect rect;
			rect.bottom_left = pen;
			rect.dimensions = dimensions;
			dialog->records_bounds[i] = rect;
			pen.y -= advance;
		}
	}

	dialog->record_hovered = invalid_index;
	dialog->record_selected = invalid_index;
	dialog->path_button_selected = invalid_index;
	dialog->pick.enabled = false;
	dialog->scroll_top = 0.0f;
}

static void open_dialog(FilePickDialog* dialog, bmfont::Font* font, Heap* heap)
{
	dialog->bounds.bottom_left = {-350.0f, -250.0f};
	dialog->bounds.dimensions = {400.0f, 500.0f};

	dialog->path_button_padding = {1.0f, 1.0f, 1.0f, 1.0f};
	dialog->path_button_margin = {5.0f, 5.0f, 0.0f, 5.0f + font->line_height};
	dialog->record_padding = {1.0f, 1.0f, 1.0f, 1.0f};
	dialog->record_margin = {3.0f, 1.0f, 3.0f, 1.0f};
	dialog->footer_padding = {5.0f, 10.0f, 5.0f, 10.0f};

	// Layout the pick button.
	{
		dialog->pick.label = "Open File";

		Item* item = &dialog->pick.item;
		Spacing padding = {1.0f, 1.0f, 1.0f, 1.0f};
		item->padding = padding;

		Vector2 text_bounds = compute_text_bounds(dialog->pick.label, font);
		Vector2 dimensions;
		dimensions.x = padding.start + text_bounds.x + padding.end;
		dimensions.y = padding.top + text_bounds.y + padding.bottom;
		item->bounds.dimensions = dimensions;

		float right = dialog->bounds.bottom_left.x + dialog->bounds.dimensions.x;
		float bottom = dialog->bounds.bottom_left.y;
		item->bounds.bottom_left.x = right - dimensions.x;
		item->bounds.bottom_left.y = bottom + padding.bottom;
	}

	const char* default_path = get_documents_folder(heap);
	open_directory(dialog, default_path, font, heap);
}

static void close_dialog(FilePickDialog* dialog, Heap* heap)
{
	if(dialog)
	{
		FOR_N(i, dialog->path_buttons_count)
		{
			SAFE_HEAP_DEALLOCATE(heap, dialog->path_buttons[i].text);
		}
		dialog->path_buttons_count = 0;
		SAFE_HEAP_DEALLOCATE(heap, dialog->path_buttons);
		SAFE_HEAP_DEALLOCATE(heap, dialog->path);
		SAFE_HEAP_DEALLOCATE(heap, dialog->records_bounds);
		destroy_directory(&dialog->directory, heap);
		dialog->enabled = false;
	}
}

struct Viewport
{
	int width;
	int height;
};

static void draw_dialog(FilePickDialog* dialog, bmfont::Font* font, Viewport* viewport)
{
	const Vector4 backdrop_colour = {0.0f, 0.0f, 0.0f, 0.9f};
	const Vector4 hover_colour = {1.0f, 1.0f, 1.0f, 0.3f};
	const Vector4 selection_colour = {1.0f, 1.0f, 1.0f, 0.5f};
	const Vector4 disabled_button_colour = {0.2f, 0.2f, 0.2f, 1.0f};

	// Draw the backdrop.
	immediate::draw_transparent_rect(dialog->bounds, backdrop_colour);

	// Draw the selection highlight for a selected path button.
	if(dialog->path_button_selected != invalid_index)
	{
		PathButton button = dialog->path_buttons[dialog->path_button_selected];
		immediate::draw_transparent_rect(button.bounds, hover_colour);
	}

	// Draw buttons for each segment in the current directory's path.
	FOR_N(i, dialog->path_buttons_count)
	{
		PathButton button = dialog->path_buttons[i];
		draw_text(button.text, button.bounds.bottom_left, font);
	}

	// Clip anything that scrolls outside the middle area of the window.
	float scroll_area_height = get_inner_height(dialog, font);
	Rect clip_area = dialog->bounds;
	clip_area.dimensions.y = scroll_area_height;
	clip_area.bottom_left.y += get_footer_height(dialog, font);
	immediate::set_clip_area(clip_area, viewport->width, viewport->height);

	// Draw the hover highlight for the records.
	if(dialog->record_hovered != invalid_index && dialog->record_hovered != dialog->record_selected)
	{
		Rect rect = dialog->records_bounds[dialog->record_hovered];
		rect.bottom_left.y += dialog->scroll_top;
		immediate::draw_transparent_rect(rect, hover_colour);
	}

	// Draw the selection highlight for the records.
	if(dialog->record_selected != invalid_index)
	{
		Rect rect = dialog->records_bounds[dialog->record_selected];
		rect.bottom_left.y += dialog->scroll_top;
		immediate::draw_transparent_rect(rect, selection_colour);
	}

	// Draw a line for each file in the current directory.
	float record_height = get_record_height(dialog, font);
	float scroll = dialog->scroll_top;
	int start_index = scroll / record_height;
	start_index = MAX(start_index, 0);
	int end_index = ceil((scroll + scroll_area_height) / record_height);
	end_index = MIN(end_index, dialog->directory.records_count);

	for(int i = start_index; i < end_index; i += 1)
	{
		DirectoryRecord record = dialog->directory.records[i];
		char* name = record.name;

		Rect bounds = dialog->records_bounds[i];
		Vector2 start = bounds.bottom_left;
		start.y += dialog->scroll_top;

		draw_text(name, start, font);
	}

	immediate::stop_clip_area();

	// Draw the footer.
	Spacing padding = dialog->footer_padding;
	Rect rect;
	rect.bottom_left = dialog->bounds.bottom_left;
	rect.dimensions.x = dialog->bounds.dimensions.x;
	rect.dimensions.y = padding.top + font->line_height + padding.bottom;
	immediate::draw_opaque_rect(rect, {0.0f, 0.0f, 0.0f, 1.0f});

	if(dialog->record_selected != invalid_index)
	{
		Vector2 start;
		start.x = rect.bottom_left.x + padding.start;
		start.y = rect.bottom_left.y + padding.bottom;
		DirectoryRecord record = dialog->directory.records[dialog->record_selected];
		draw_text(record.name, start, font);
	}

	Button pick = dialog->pick;
	if(!pick.enabled)
	{
		immediate::draw_opaque_rect(pick.item.bounds, disabled_button_colour);
	}
	else if(pick.hovered)
	{
		immediate::draw_transparent_rect(pick.item.bounds, hover_colour);
	}
	draw_text(pick.label, pick.item.bounds.bottom_left, font);
}

static void touch_record(FilePickDialog* dialog, int record_index, bmfont::Font* font, Heap* heap)
{
	DirectoryRecord record = dialog->directory.records[record_index];
	switch(record.type)
	{
		case DirectoryRecordType::Directory:
		{
			char* path = append_to_path(dialog->path, record.name, heap);

			close_dialog(dialog, heap);
			open_directory(dialog, path, font, heap);

			HEAP_DEALLOCATE(heap, path);
			break;
		}
		case DirectoryRecordType::File:
		{
			dialog->record_selected = record_index;
			dialog->pick.enabled = true;
			break;
		}
		default:
		{
			break;
		}
	}
}

static void open_parent_directory(FilePickDialog* dialog, int segment, bmfont::Font* font, Heap* heap)
{
	const char* path = dialog->path;
	int slash = 0;
	if(segment == 0)
	{
		// the root directory case
		if(string_size(path) == 0)
		{
			slash = 1;
		}
		else
		{
			slash = 2;
		}
	}
	else
	{
		FOR_N(i, segment + 1)
		{
			int found_index = find_char(&path[slash], '/');
			if(found_index != -1)
			{
				slash += found_index + 1;
			}
			else
			{
				slash = string_size(path) + 1;
			}
		}
	}
	int subpath_size = slash;
	char* subpath = HEAP_ALLOCATE(heap, char, subpath_size);
	copy_string(subpath, subpath_size, path);

	close_dialog(dialog, heap);
	open_directory(dialog, subpath, font, heap);

	HEAP_DEALLOCATE(heap, subpath);
}

void pick_file(FilePickDialog* dialog, const char* name, Heap* heap, Stack* stack);

static void detect_mouse_selection(FilePickDialog* dialog, Vector2 mouse_position, bool clicked, bmfont::Font* font, Heap* heap, Stack* stack)
{
	if(!point_in_rect(dialog->bounds, mouse_position))
	{
		return;
	}

	dialog->path_button_selected = invalid_index;
	FOR_N(i, dialog->path_buttons_count)
	{
		PathButton button = dialog->path_buttons[i];
		if(point_in_rect(button.bounds, mouse_position))
		{
			dialog->path_button_selected = i;
			if(clicked)
			{
				open_parent_directory(dialog, i, font, heap);
			}
			break;
		}
	}

	float record_height = get_record_height(dialog, font);
	float scroll_area_height = get_inner_height(dialog, font);
	float scroll = dialog->scroll_top;
	int start_index = scroll / record_height;
	start_index = MAX(start_index, 0);
	int end_index = ceil((scroll + scroll_area_height) / record_height);
	end_index = MIN(end_index, dialog->directory.records_count);

	dialog->record_hovered = invalid_index;
	for(int i = start_index; i < end_index; i += 1)
	{
		Rect bounds = dialog->records_bounds[i];
		bounds.bottom_left.y += dialog->scroll_top;
		if(point_in_rect(bounds, mouse_position))
		{
			dialog->record_hovered = i;
			if(clicked)
			{
				touch_record(dialog, i, font, heap);
			}
			break;
		}
	}

	Button* pick = &dialog->pick;
	pick->hovered = false;
	if(pick->enabled && point_in_rect(pick->item.bounds, mouse_position))
	{
		pick->hovered = true;
		if(clicked)
		{
			int selected = dialog->record_selected;
			if(selected != invalid_index)
			{
				DirectoryRecord record = dialog->directory.records[selected];
				pick_file(dialog, record.name, heap, stack);
			}
		}
	}
}

static float get_scroll_bottom(FilePickDialog* dialog, bmfont::Font* font)
{
	float inner_height = get_inner_height(dialog, font);
	float lines = dialog->directory.records_count;
	float content_height = lines * get_record_height(dialog, font);
	return fmax(content_height - inner_height, 0.0f);
}

static void set_scroll(FilePickDialog* dialog, float scroll, bmfont::Font* font)
{
	float scroll_bottom = get_scroll_bottom(dialog, font);
	dialog->scroll_top = clamp(scroll, 0.0f, scroll_bottom);
}

static void scroll(FilePickDialog* dialog, float scroll_velocity_y, bmfont::Font* font)
{
	const float speed = 120.0f;
	float scroll_top = dialog->scroll_top - (speed * scroll_velocity_y);
	set_scroll(dialog, scroll_top, font);
}

namespace
{
	const Vector3 vector3_red     = {1.0f, 0.0f, 0.0f};
	const Vector3 vector3_green   = {0.0f, 1.0f, 0.0f};
	const Vector3 vector3_blue    = {0.0f, 0.0f, 1.0f};
	const Vector3 vector3_cyan    = {0.0f, 1.0f, 1.0f};
	const Vector3 vector3_magenta = {1.0f, 0.0f, 1.0f};
	const Vector3 vector3_yellow  = {1.0f, 1.0f, 0.0f};
	const Vector3 vector3_black   = {0.0f, 0.0f, 0.0f};
	const Vector3 vector3_white   = {1.0f, 1.0f, 1.0f};

	const Vector4 vector4_red     = {1.0f, 0.0f, 0.0f, 1.0f};
	const Vector4 vector4_green   = {0.0f, 1.0f, 0.0f, 1.0f};
	const Vector4 vector4_blue    = {0.0f, 0.0f, 1.0f, 1.0f};
	const Vector4 vector4_cyan    = {0.0f, 1.0f, 1.0f, 1.0f};
	const Vector4 vector4_magenta = {1.0f, 0.0f, 1.0f, 1.0f};
	const Vector4 vector4_yellow  = {1.0f, 1.0f, 0.0f, 1.0f};
	const Vector4 vector4_black   = {0.0f, 0.0f, 0.0f, 1.0f};
	const Vector4 vector4_white   = {1.0f, 1.0f, 1.0f, 1.0f};

	const float near_plane = 0.001f;
	const float far_plane = 100.0f;

	GLuint nearest_repeat;

	struct
	{
		GLuint program;
		GLint model_view_projection;
	} shader_vertex_colour;

	struct
	{
		GLuint program;
		GLint model_view_projection;
		GLint texture;
	} shader_texture_only;

	struct
	{
		GLuint program;
		GLint model_view_projection;
		GLint texture;
		GLint text_colour;
	} shader_font;

	struct
	{
		GLuint program;
		GLint light_direction;
		GLint model_view_projection;
		GLint normal_matrix;
	} shader_lit;

	struct
	{
		GLuint program;
		GLint model_view_projection;
	} shader_halo;

	Object sky;
	Object dodecahedron;
	Object test_model;
	Object imported_model;

	jan::Mesh test_mesh;
	jan::Selection selection;

	Viewport viewport;
	Matrix4 projection;
	Matrix4 sky_projection;
	Matrix4 screen_projection;
	Stack scratch;
	Heap heap;

	struct
	{
		Vector3 position;
		Vector3 target;
	} camera;

	struct
	{
		Vector2 position;
		Vector2 velocity;
		float scroll_velocity_y;
		input::MouseButton button;
		bool drag;
	} mouse;

	bool translating;

	FilePickDialog dialog;
	bmfont::Font font;
	GLuint font_textures[1];

	ui::Item test_container;
	ui::Item dialog_panel;
	ui::Item* focused_container;
}

bool system_startup()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	stack_create(&scratch, MEBIBYTES(16));
	heap_create(&heap, MEBIBYTES(16));

	// Setup samplers.
	{
		glGenSamplers(1, &nearest_repeat);
		glSamplerParameteri(nearest_repeat, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(nearest_repeat, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(nearest_repeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(nearest_repeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	// Vertex Colour Shader
	shader_vertex_colour.program = load_shader_program(vertex_source_vertex_colour, fragment_source_vertex_colour, &scratch);
	if(shader_vertex_colour.program == 0)
	{
		LOG_ERROR("The vertex colour shader failed to load.");
		return false;
	}
	{
		shader_vertex_colour.model_view_projection = glGetUniformLocation(shader_vertex_colour.program, "model_view_projection");
	}

	// Texture Only Shader
	shader_texture_only.program = load_shader_program(vertex_source_texture_only, fragment_source_texture_only, &scratch);
	if(shader_texture_only.program == 0)
	{
		LOG_ERROR("The texture-only shader failed to load.");
		return false;
	}
	{
		GLuint program = shader_texture_only.program;
		shader_texture_only.model_view_projection = glGetUniformLocation(program, "model_view_projection");
		shader_texture_only.texture = glGetUniformLocation(program, "texture");

		glUseProgram(shader_texture_only.program);
		glUniform1i(shader_texture_only.texture, 0);
	}

	// Font Shader
	shader_font.program = load_shader_program(vertex_source_font, fragment_source_font, &scratch);
	if(shader_font.program == 0)
	{
		LOG_ERROR("The font shader failed to load.");
		return false;
	}
	{
		GLuint program = shader_font.program;
		shader_font.model_view_projection = glGetUniformLocation(program, "model_view_projection");
		shader_font.texture = glGetUniformLocation(program, "texture");
		shader_font.text_colour = glGetUniformLocation(program, "text_colour");

		glUseProgram(shader_font.program);
		glUniform1i(shader_font.texture, 0);
		const Vector3 text_colour = vector3_one;
		glUniform3fv(shader_font.text_colour, 1, &text_colour[0]);
	}

	// Lit Shader
	shader_lit.program = load_shader_program(lit_vertex_source, lit_fragment_source, &scratch);
	if(shader_lit.program == 0)
	{
		LOG_ERROR("The lit shader failed to load.");
		return false;
	}
	{
		GLuint program = shader_lit.program;
		shader_lit.model_view_projection = glGetUniformLocation(program, "model_view_projection");
		shader_lit.normal_matrix = glGetUniformLocation(program, "normal_matrix");
		shader_lit.light_direction = glGetUniformLocation(program, "light_direction");
	}

	// Halo Shader
	shader_halo.program = load_shader_program(vertex_source_halo, fragment_source_halo, &scratch);
	if(shader_halo.program == 0)
	{
		LOG_ERROR("The halo shader failed to load.");
		return false;
	}
	{
		GLuint program = shader_halo.program;
		shader_halo.model_view_projection = glGetUniformLocation(program, "model_view_projection");
	}

	// Sky
	object_create(&sky);
	object_generate_sky(&sky, &scratch);

	// Dodecahedron
	{
		object_create(&dodecahedron);
		jan::Mesh mesh;
		jan::create_mesh(&mesh);
		jan::make_a_weird_face(&mesh);

		jan::Selection selection = jan::select_all(&mesh, &heap);
		jan::extrude(&mesh, &selection, 0.6f, &scratch);
		jan::destroy_selection(&selection);

		jan::colour_all_faces(&mesh, vector3_cyan);

		VertexPNC* vertices;
		int vertices_count;
		u16* indices;
		int indices_count;
		jan::triangulate(&mesh, &heap, &vertices, &vertices_count, &indices, &indices_count);
		object_set_surface(&dodecahedron, vertices, vertices_count, indices, indices_count);
		HEAP_DEALLOCATE(&heap, vertices);
		HEAP_DEALLOCATE(&heap, indices);

		obj::save_file("weird.obj", &mesh, &heap);
		jan::destroy_mesh(&mesh);
	}

	// Camera
	camera.position = {-4.0f, -4.0f, 2.0f};
	camera.target = vector3_zero;

	// Test Model
	{
		obj::load_file("test.obj", &test_mesh, &heap, &scratch);

		jan::colour_all_faces(&test_mesh, vector3_yellow);

		object_create(&test_model);

		VertexPNC* vertices;
		int vertices_count;
		u16* indices;
		int indices_count;
		jan::triangulate(&test_mesh, &heap, &vertices, &vertices_count, &indices, &indices_count);
		object_set_surface(&test_model, vertices, vertices_count, indices, indices_count);
		HEAP_DEALLOCATE(&heap, vertices);
		HEAP_DEALLOCATE(&heap, indices);

		jan::create_selection(&selection, &heap);
		selection.type = jan::Selection::Type::Face;
	}

	// Imported model
	{
		object_create(&imported_model);
	}

	// Droid font
	{
		bmfont::load_font(&font, "droid_12.fnt", &heap, &scratch);

		for(int i = 0; i < font.pages_count; i += 1)
		{
			const char* path = font.pages[i].bitmap_filename;
			Bitmap bitmap;
			bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
			font_textures[i] = upload_bitmap(&bitmap);
			stbi_image_free(bitmap.pixels);
		}
	}

	immediate::context_create(&heap);
	immediate::set_shader(shader_vertex_colour.program);
	immediate::set_textured_shader(shader_font.program);

	// Test UI prototype.
	{
		test_container.type = ui::ItemType::Container;
		test_container.growable = true;
		test_container.container.background_colour = {0.145f, 0.145f, 0.145f, 1.0f};
		test_container.container.padding = {1.0f, 1.0f, 1.0f, 1.0f};
		test_container.container.direction = ui::Direction::Left_To_Right;
		test_container.container.alignment = ui::Alignment::Start;
		test_container.container.justification = ui::Justification::Start;

		const int items_in_row = 2;
		ui::add_row(&test_container.container, items_in_row, &heap);

		test_container.container.items[0].type = ui::ItemType::Button;
		test_container.container.items[0].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
		test_container.container.items[0].button.text_block.text = "Import .obj";
		test_container.container.items[0].button.text_block.font = &font;

		test_container.container.items[1].type = ui::ItemType::Button;
		test_container.container.items[1].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
		test_container.container.items[1].button.text_block.text = "Export .obj";
		test_container.container.items[1].button.text_block.font = &font;
	}

	return true;
}

void system_shutdown(bool functions_loaded)
{
	if(functions_loaded)
	{
		glDeleteSamplers(1, &nearest_repeat);

		glDeleteProgram(shader_vertex_colour.program);
		glDeleteProgram(shader_texture_only.program);
		glDeleteProgram(shader_font.program);
		glDeleteProgram(shader_lit.program);
		glDeleteProgram(shader_halo.program);

		object_destroy(&sky);
		object_destroy(&dodecahedron);
		object_destroy(&test_model);
		object_destroy(&imported_model);

		jan::destroy_mesh(&test_mesh);
		jan::destroy_selection(&selection);

		bmfont::destroy_font(&font, &heap);

		immediate::context_destroy(&heap);
	}

	close_dialog(&dialog, &heap);

	ui::destroy_container(&test_container.container, &heap);

	stack_destroy(&scratch);
	heap_destroy(&heap);
}

Ray ray_from_viewport_point(Vector2 point, int viewport_width, int viewport_height, Matrix4 view, Matrix4 projection, bool orthographic)
{
	float extent_x = viewport_width / 2.0f;
	float extent_y = viewport_height / 2.0f;
	point.x = (point.x - extent_x) / extent_x;
	point.y = -(point.y - extent_y) / extent_y;

	Matrix4 inverse;
	if(orthographic)
	{
		inverse = inverse_view_matrix(view) * inverse_orthographic_matrix(projection);
	}
	else
	{
		inverse = inverse_view_matrix(view) * inverse_perspective_matrix(projection);
	}

	Vector3 near = {point.x, point.y, 1.0f};
	Vector3 far = {point.x, point.y, 0.0f};

	Vector3 start = inverse * near;
	Vector3 end = inverse * far;

	Ray result;
	result.origin = start;
	result.direction = normalise(end - start);
	return result;
}

static void focus_on_container(ui::Item* item)
{
	focused_container = item;
}

static bool focused_on(ui::Item* item)
{
	return item == focused_container;
}

void ui_handle_input(ui::Item* item)
{
	switch(item->type)
	{
		case ui::ItemType::Container:
		{
			ui::Container* container = &item->container;
			FOR_N(i, container->items_count)
			{
				ui_handle_input(&container->items[i]);
			}
			break;
		}
		case ui::ItemType::Text_Block:
		{
			break;
		}
		case ui::ItemType::Button:
		{
			ui::Button* button = &item->button;
			if(button->hovered && input::get_mouse_clicked(input::MouseButton::Left))
			{
				open_dialog(&dialog, &font, &heap);
				dialog_panel.bounds = dialog.bounds;
				focus_on_container(&dialog_panel);
				dialog_panel.unfocusable = false;
			}
			break;
		}
	}
}

static void detect_focus_changes_for_toplevel_containers()
{
	bool clicked =
		input::get_mouse_clicked(input::MouseButton::Left) ||
		input::get_mouse_clicked(input::MouseButton::Middle) ||
		input::get_mouse_clicked(input::MouseButton::Right);

	Vector2 mouse_position;
	mouse_position.x = mouse.position.x - viewport.width / 2.0f;
	mouse_position.y = -(mouse.position.y - viewport.height / 2.0f);

	const int items_count = 2;
	ui::Item* items[items_count] =
	{
		&test_container,
		&dialog_panel,
	};

	bool any_changed = false;
	FOR_N(i, items_count)
	{
		ui::Item* item = items[i];
		bool hovered = point_in_rect(item->bounds, mouse_position);
		if(hovered)
		{
			if(clicked && !item->unfocusable && !any_changed)
			{
				focused_container = item;
				any_changed = true;
			}
			ui::detect_hover(item, mouse_position);
		}
	}
	if(clicked && !any_changed)
	{
		focused_container = nullptr;
	}
}

void system_update()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Interpret user input.
	if(input::get_mouse_clicked(input::MouseButton::Left))
	{
		mouse.drag = true;
		mouse.button = input::MouseButton::Left;
	}
	if(input::get_mouse_clicked(input::MouseButton::Right))
	{
		mouse.drag = true;
		mouse.button = input::MouseButton::Right;
	}
	if(mouse.drag)
	{
		if(
			input::get_mouse_pressed(input::MouseButton::Left) ||
			input::get_mouse_pressed(input::MouseButton::Right))
		{
			int velocity_x;
			int velocity_y;
			input::get_mouse_velocity(&velocity_x, &velocity_y);
			mouse.velocity.x = velocity_x;
			mouse.velocity.y = velocity_y;
		}
		else
		{
			mouse.drag = false;
		}
	}
	{
		int position_x;
		int position_y;
		input::get_mouse_position(&position_x, &position_y);
		mouse.position.x = position_x;
		mouse.position.y = position_y;

		const float scroll_speed = 0.15f;
		int scroll_velocity_x;
		int scroll_velocity_y;
		input::get_mouse_scroll_velocity(&scroll_velocity_x, &scroll_velocity_y);
		mouse.scroll_velocity_y = scroll_speed * scroll_velocity_y;
	}

	detect_focus_changes_for_toplevel_containers();

	// Update the file pick dialog.
	if(dialog.enabled)
	{
		if(focused_on(&dialog_panel))
		{
			Vector2 mouse_position;
			mouse_position.x = mouse.position.x - viewport.width / 2.0f;
			mouse_position.y = -(mouse.position.y - viewport.height / 2.0f);
			bool clicked = input::get_mouse_clicked(input::MouseButton::Left);
			detect_mouse_selection(&dialog, mouse_position, clicked, &font, &heap, &scratch);
			scroll(&dialog, mouse.scroll_velocity_y, &font);
		}
		else
		{
			close_dialog(&dialog, &heap);
			dialog_panel.unfocusable = true;
		}
	}

	if(focused_on(&test_container))
	{
		ui_handle_input(&test_container);
	}

	if(!focused_container)
	{
		if(input::get_key_tapped(input::Key::G))
		{
			translating = !translating;
		}

		// Update the camera.
		Vector3 forward = camera.position - camera.target;
		Vector3 right = normalise(cross(vector3_unit_z, forward));
		Vector3 up = normalise(cross(right, forward));
		if(mouse.scroll_velocity_y != 0.0f)
		{
			Vector3 moved = (mouse.scroll_velocity_y * -forward) + camera.position;
			Vector3 forward_moved = moved - camera.target;
			const float too_close = 0.1f;
			const float too_far = 40.0f;
			if(dot(forward_moved, forward) < 0.0f)
			{
				// If this would move the camera past the target, instead place
				// it at a close threshold.
				camera.position = (too_close * normalise(forward)) + camera.target;
			}
			else if(length(forward_moved) > too_far)
			{
				camera.position = (too_far * normalise(forward)) + camera.target;
			}
			else
			{
				camera.position = moved;
			}
		}
		if(mouse.drag && !translating)
		{
			switch(mouse.button)
			{
				case input::MouseButton::Left:
				{
					const Vector2 orbit_speed = {0.01f, 0.01f};
					Vector2 angular_velocity = pointwise_multiply(orbit_speed, -mouse.velocity);
					Quaternion orbit_x = axis_angle_rotation(vector3_unit_z, angular_velocity.x);
					Quaternion orbit_y = axis_angle_rotation(right, angular_velocity.y);
					if(dot(cross(forward, vector3_unit_z), cross(orbit_y * forward, vector3_unit_z)) < 0.0f)
					{
						// Prevent from orbiting over the pole (Z axis), because
						// it creates an issue where next frame it would orient
						// the camera relative to the axis it had just passed
						// and flip the camera view over. Furthermore, since
						// you're usually still moving the mouse it'll move back
						// toward the pole next frame, and flip again over and
						// over every frame.
						orbit_y = quaternion_identity;
					}
					camera.position = (orbit_y * orbit_x * forward) + camera.target;
					break;
				}
				case input::MouseButton::Right:
				{
					// Cast rays into world space corresponding to the mouse
					// position for this frame and the frame prior.
					Vector2 prior_position = mouse.position + mouse.velocity;
					Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
					Ray prior = ray_from_viewport_point(prior_position, viewport.width, viewport.height, view, projection, false);
					Ray current = ray_from_viewport_point(mouse.position, viewport.width, viewport.height, view, projection, false);

					// Project the rays onto the plane containing the camera
					// target and facing the camera.
					Vector3 n = normalise(camera.position - camera.target);
					float d0 = dot(camera.target - current.origin, n) / dot(n, current.direction);
					float d1 = dot(camera.target - prior.origin, n) / dot(n, prior.direction);
					Vector3 p0 = (d0 * current.direction) + current.origin;
					Vector3 p1 = (d1 * prior.direction) + prior.origin;

					// Pan by the amount moved across that plane.
					Vector3 pan = p0 - p1;
					camera.position += pan;
					camera.target += pan;
					break;
				}
				default:
				{
					break;
				}
			}
		}

		// Translation of faces.
		if(translating)
		{
			int velocity_x;
			int velocity_y;
			input::get_mouse_velocity(&velocity_x, &velocity_y);
			mouse.velocity.x = velocity_x;
			mouse.velocity.y = velocity_y;

			const Vector2 move_speed = {0.007f, 0.007f};
			Vector2 move_velocity = pointwise_multiply(move_speed, mouse.velocity);
			Vector3 move = (move_velocity.x * right) + (move_velocity.y * up);
			move_faces(&test_mesh, &selection, move);
		}

		// Cast a ray from the mouse to the test model.
		{
			jan::colour_all_faces(&test_mesh, vector3_yellow);
			jan::colour_selection(&test_mesh, &selection, vector3_cyan);

			Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
			Ray ray = ray_from_viewport_point(mouse.position, viewport.width, viewport.height, view, projection, false);
			jan::Face* face = first_face_hit_by_ray(&test_mesh, ray);
			if(face)
			{
				if(input::get_key_tapped(input::Key::F))
				{
					toggle_face_in_selection(&selection, face);
					jan::colour_just_the_one_face(face, vector3_cyan);
				}
				else
				{
					jan::colour_just_the_one_face(face, vector3_red);
				}
			}
			object_update_mesh(&test_model, &test_mesh, &heap);
		}
	}

	// Update matrices.
	{
		Matrix4 model = matrix4_identity;

		Vector3 direction = normalise(camera.target - camera.position);
		Matrix4 view = look_at_matrix(vector3_zero, direction, vector3_unit_z);
		object_set_matrices(&sky, model, view, sky_projection);

		model = compose_transform({2.0f, 0.0f, 0.0f}, quaternion_identity, vector3_one);
		view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
		object_set_matrices(&dodecahedron, model, view, projection);

		model = matrix4_identity;
		object_set_matrices(&test_model, model, view, projection);

		model = compose_transform({-2.0f, 0.0f, 0.0f}, quaternion_identity, vector3_one);
		object_set_matrices(&imported_model, model, view, projection);

		immediate::set_matrices(view, projection);

		// Update light parameters.

		Vector3 light_direction = {0.7f, 0.4f, -1.0f};
		light_direction = normalise(-(view * light_direction));

		glUseProgram(shader_lit.program);
		glUniform3fv(shader_lit.light_direction, 1, &light_direction[0]);
	}

	glUseProgram(shader_lit.program);

	// Draw the dodecahedron.
	{
		glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, dodecahedron.model_view_projection.elements);
		glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, dodecahedron.normal.elements);
		glBindVertexArray(dodecahedron.vertex_array);
		glDrawElements(GL_TRIANGLES, dodecahedron.indices_count, GL_UNSIGNED_SHORT, nullptr);
	}

	// Draw the test model.
	{
		glEnable(GL_STENCIL_TEST);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		glStencilMask(0xff);
		glClear(GL_STENCIL_BUFFER_BIT);

		glStencilFunc(GL_ALWAYS, 1, 0xff);

		glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, test_model.model_view_projection.elements);
		glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, test_model.normal.elements);
		glBindVertexArray(test_model.vertex_array);
		glDrawElements(GL_TRIANGLES, test_model.indices_count, GL_UNSIGNED_SHORT, nullptr);
	}

	glUseProgram(shader_halo.program);

	// Draw the test model's halo.
	{
		glStencilFunc(GL_NOTEQUAL, 1, 0xff);
		glStencilMask(0x00);

		glLineWidth(3.0f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glUniformMatrix4fv(shader_halo.model_view_projection, 1, GL_TRUE, test_model.model_view_projection.elements);
		glBindVertexArray(test_model.vertex_array);
		glDrawElements(GL_TRIANGLES, test_model.indices_count, GL_UNSIGNED_SHORT, nullptr);

		glLineWidth(1.0f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_STENCIL_TEST);
	}

	glUseProgram(shader_lit.program);

	// Draw the imported model.
	if(imported_model.indices_count)
	{
		glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, imported_model.model_view_projection.elements);
		glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, imported_model.normal.elements);
		glBindVertexArray(imported_model.vertex_array);
		glDrawElements(GL_TRIANGLES, imported_model.indices_count, GL_UNSIGNED_SHORT, nullptr);
	}

	// Draw the sky behind everything else.
	{
		glDepthFunc(GL_EQUAL);
		glDepthRange(1.0, 1.0);
		glUseProgram(shader_vertex_colour.program);
		glUniformMatrix4fv(shader_vertex_colour.model_view_projection, 1, GL_TRUE, sky.model_view_projection.elements);
		glBindVertexArray(sky.vertex_array);
		glDrawElements(GL_TRIANGLES, sky.indices_count, GL_UNSIGNED_SHORT, nullptr);
		glDepthFunc(GL_LESS);
		glDepthRange(0.0, 1.0);
	}

	// Draw screen-space debug UI.
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	Matrix4 screen_view = matrix4_identity;
	immediate::set_matrices(screen_view, screen_projection);

	// Test drawing text.
	if(dialog.enabled)
	{
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, font_textures[0]);
		glBindSampler(0, nearest_repeat);

		draw_dialog(&dialog, &font, &viewport);
	}

	// Test UI prototypes.
	{
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, font_textures[0]);
		glBindSampler(0, nearest_repeat);

		Rect space;
		space.bottom_left = {-viewport.width / 2.0f, viewport.height / 2.0f};
		space.dimensions = {viewport.width, 60.0f};
		space.bottom_left.y -= space.dimensions.y;
		ui::lay_out(&test_container, space);
		ui::draw(&test_container);
	}

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	// Output a test screenshot.
	if(input::get_key_tapped(input::Key::Space))
	{
		int pixels_count = viewport.width * viewport.height;
		Pixel24* pixels = STACK_ALLOCATE(&scratch, Pixel24, pixels_count);
		glReadPixels(0, 0, viewport.width, viewport.height, GL_BGR, GL_UNSIGNED_BYTE, pixels);
		bmp::write_file("test.bmp", reinterpret_cast<const u8*>(pixels), viewport.width, viewport.height);
		STACK_DEALLOCATE(&scratch, pixels);
	}
}

void pick_file(FilePickDialog* dialog, const char* name, Heap* heap, Stack* stack)
{
	char* path = append_to_path(dialog->path, name, heap);
	jan::Mesh mesh;
	bool loaded = obj::load_file(path, &mesh, heap, stack);
	HEAP_DEALLOCATE(heap, path);

	if(!loaded)
	{
		// TODO: Report this to the user, not the log!
		LOG_DEBUG("Failed to load the file %s as an .obj.", name);
	}
	else
	{
		jan::colour_all_faces(&mesh, vector3_magenta);
		VertexPNC* vertices;
		int vertices_count;
		u16* indices;
		int indices_count;
		jan::triangulate(&mesh, heap, &vertices, &vertices_count, &indices, &indices_count);
		object_set_surface(&imported_model, vertices, vertices_count, indices, indices_count);
		HEAP_DEALLOCATE(heap, vertices);
		HEAP_DEALLOCATE(heap, indices);
		jan::destroy_mesh(&mesh);

		close_dialog(dialog, heap);
		dialog_panel.unfocusable = true;
	}
}

void resize_viewport(int width, int height)
{
	viewport.width = width;
	viewport.height = height;

	const float fov = pi_over_2 * (2.0f / 3.0f);
	projection = perspective_projection_matrix(fov, width, height, near_plane, far_plane);
	sky_projection = perspective_projection_matrix(fov, width, height, 0.001f, 1.0f);
	screen_projection = orthographic_projection_matrix(width, height, -1.0f, 1.0f);
	glViewport(0, 0, width, height);
}

} // namespace video
