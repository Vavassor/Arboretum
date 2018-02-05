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
#include "colours.h"
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

struct FilePickDialog
{
	Directory directory;
	char* path;
	ui::Item* panel;
	ui::Id* path_buttons;
	ui::TextBlock* file_readout;
	ui::Id pick_button;
	ui::Button* pick;
	int path_buttons_count;
	int record_selected;
	bool show_hidden_records;
	bool enabled;
};

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

static void open_directory(FilePickDialog* dialog, const char* directory, bmfont::Font* font, ui::Context* context, Heap* heap)
{
	dialog->enabled = true;

	// Create the panel for the dialog box.
	ui::Item* panel = ui::create_toplevel_container(context, heap);
	panel->type = ui::ItemType::Container;
	panel->growable = true;
	panel->container.alignment = ui::Alignment::Stretch;
	dialog->panel = panel;

	ui::add_column(&panel->container, 3, context, heap);

	// Set up the path bar at the top of the dialog.
	dialog->path = copy_string_onto_heap(directory, heap);

	int slashes = count_char_occurrences(dialog->path, '/');
	int buttons_in_row = slashes + 1;
	dialog->path_buttons_count = buttons_in_row;
	dialog->path_buttons = HEAP_ALLOCATE(heap, ui::Id, buttons_in_row);

	ui::Item* path_bar = &panel->container.items[0];
	path_bar->type = ui::ItemType::Container;
	path_bar->container.background_colour = vector4_yellow;
	ui::add_row(&path_bar->container, buttons_in_row, context, heap);

	// Add buttons to the path bar.
	int i = 0;
	for(const char* path = dialog->path; *path; i += 1)
	{
		ui::Item* item = &path_bar->container.items[i];
		item->type = ui::ItemType::Button;

		ui::Button* button = &item->button;
		button->text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
		button->text_block.font = font;
		dialog->path_buttons[i] = item->id;

		int found_index = find_char(path, '/');
		if(found_index == 0)
		{
			// root directory
			const char* name = "Filesystem";
			button->text_block.text = copy_string_to_heap(name, string_size(name), heap);
		}
		else if(found_index == -1)
		{
			// final path segment
			button->text_block.text = copy_string_to_heap(path, string_size(path), heap);
			break;
		}
		else
		{
			button->text_block.text = copy_string_to_heap(path, found_index, heap);
		}
		path += found_index + 1;
	}

	// Set up the directory listing.
	ui::Item* file_list = &panel->container.items[1];
	file_list->type = ui::ItemType::List;
	file_list->growable = true;

	ui::List* list = &file_list->list;
	list->item_spacing = 2.0f;
	list->side_margin = 2.0f;

	bool listed = list_files_in_directory(dialog->path, &dialog->directory, heap);
	ASSERT(listed);

	const int extensions_count = 1;
	const char* extensions[extensions_count] = {".obj"};
	filter_directory(&dialog->directory, extensions, extensions_count, dialog->show_hidden_records, heap);

	ui::create_items(file_list, dialog->directory.records_count, heap);

	if(dialog->directory.records_count == 0)
	{
		// Listing the directory succeeded, it's just empty.
	}
	else
	{
		quick_sort_by_filename(dialog->directory.records, dialog->directory.records_count);

		FOR_N(i, dialog->directory.records_count)
		{
			DirectoryRecord record = dialog->directory.records[i];
			ui::TextBlock* text_block = &list->items[i];
			text_block->text = copy_string_onto_heap(record.name, heap);
			text_block->padding = {1.0f, 1.0f, 1.0f, 1.0f};
			text_block->font = font;
		}
	}

	// Set up the footer.
	ui::Item* footer = &panel->container.items[2];
	footer->type = ui::ItemType::Container;
	footer->container.background_colour = vector4_blue;
	footer->container.justification = ui::Justification::Space_Between;

	ui::add_row(&footer->container, 2, context, heap);

	ui::Item* file_readout = &footer->container.items[0];
	file_readout->type = ui::ItemType::Text_Block;
	file_readout->text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
	file_readout->text_block.text = copy_string_onto_heap("", heap);
	file_readout->text_block.font = font;
	dialog->file_readout = &file_readout->text_block;

	ui::Item* pick = &footer->container.items[1];
	pick->type = ui::ItemType::Button;
	pick->button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
	pick->button.text_block.text = copy_string_onto_heap("Import", heap);
	pick->button.text_block.font = font;
	dialog->pick_button = pick->id;
	dialog->pick = &pick->button;

	ui::focus_on_container(context, panel);
}

static void open_dialog(FilePickDialog* dialog, bmfont::Font* font, ui::Context* context, Heap* heap)
{
	const char* default_path = get_documents_folder(heap);
	open_directory(dialog, default_path, font, context, heap);
}

static void close_dialog(FilePickDialog* dialog, ui::Context* context, Heap* heap)
{
	if(dialog)
	{
		SAFE_HEAP_DEALLOCATE(heap, dialog->path);
		ui::destroy_toplevel_container(context, dialog->panel, heap);
		destroy_directory(&dialog->directory, heap);
		dialog->enabled = false;
	}
}

struct Viewport
{
	int width;
	int height;
};

static void touch_record(FilePickDialog* dialog, int record_index, bmfont::Font* font, ui::Context* context, Heap* heap)
{
	DirectoryRecord record = dialog->directory.records[record_index];
	switch(record.type)
	{
		case DirectoryRecordType::Directory:
		{
			char* path = append_to_path(dialog->path, record.name, heap);

			close_dialog(dialog, context, heap);
			open_directory(dialog, path, font, context, heap);

			HEAP_DEALLOCATE(heap, path);
			break;
		}
		case DirectoryRecordType::File:
		{
			dialog->record_selected = record_index;
			dialog->pick->enabled = true;
			const char* filename = dialog->directory.records[record_index].name;
			replace_string(&dialog->file_readout->text, filename, heap);
			break;
		}
		default:
		{
			break;
		}
	}
}

static void open_parent_directory(FilePickDialog* dialog, int segment, bmfont::Font* font, ui::Context* context, Heap* heap)
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

	close_dialog(dialog, context, heap);
	open_directory(dialog, subpath, font, context, heap);

	HEAP_DEALLOCATE(heap, subpath);
}

void pick_file(FilePickDialog* dialog, const char* name, ui::Context* context, Heap* heap, Stack* stack);

static void handle_input(FilePickDialog* dialog, ui::Event event, bmfont::Font* font, ui::Context* context, Heap* heap, Stack* stack)
{
	switch(event.type)
	{
		case ui::EventType::Button:
		{
			ui::Id id = event.button.id;

			FOR_N(i, dialog->path_buttons_count)
			{
				if(id == dialog->path_buttons[i])
				{
					open_parent_directory(dialog, i, font, context, heap);
				}
			}

			if(id == dialog->pick_button)
			{
				int selected = dialog->record_selected;
				DirectoryRecord record = dialog->directory.records[selected];
				pick_file(dialog, record.name, context, heap, stack);
			}
			break;
		}
		case ui::EventType::Focus_Change:
		{
			ui::Id id = event.focus_change.now_unfocused;
			if(id == dialog->panel->id)
			{
				close_dialog(dialog, context, heap);
			}
			break;
		}
		case ui::EventType::List_Selection:
		{
			int index = event.list_selection.index;
			touch_record(dialog, index, font, context, heap);
			break;
		}
	}
}

namespace
{
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

	ui::Item* main_menu;
	ui::Context ui_context;
	ui::Id import_button_id;
	ui::Id export_button_id;
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

		FOR_N(i, font.pages_count)
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

	ui::create_context(&ui_context, &heap);

	// Setup the main menu.
	{
		main_menu = ui::create_toplevel_container(&ui_context, &heap);
		main_menu->type = ui::ItemType::Container;
		main_menu->growable = true;
		main_menu->container.background_colour = {0.145f, 0.145f, 0.145f, 1.0f};
		main_menu->container.padding = {1.0f, 1.0f, 1.0f, 1.0f};
		main_menu->container.direction = ui::Direction::Left_To_Right;
		main_menu->container.alignment = ui::Alignment::Start;
		main_menu->container.justification = ui::Justification::Start;

		const int items_in_row = 2;
		ui::add_row(&main_menu->container, items_in_row, &ui_context, &heap);

		main_menu->container.items[0].type = ui::ItemType::Button;
		main_menu->container.items[0].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
		main_menu->container.items[0].button.text_block.text = copy_string_onto_heap("Import .obj", &heap);
		main_menu->container.items[0].button.text_block.font = &font;
		import_button_id = main_menu->container.items[0].id;

		main_menu->container.items[1].type = ui::ItemType::Button;
		main_menu->container.items[1].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
		main_menu->container.items[1].button.text_block.text = copy_string_onto_heap("Export .obj", &heap);
		main_menu->container.items[1].button.text_block.font = &font;
		export_button_id = main_menu->container.items[1].id;
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

	close_dialog(&dialog, &ui_context, &heap);

	ui::destroy_context(&ui_context, &heap);

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

	// Update the UI system and respond to any events that occurred.
	{
		ui::update(&ui_context);
		ui::Event event;
		while(ui::dequeue(&ui_context.queue, &event))
		{
			if(dialog.enabled)
			{
				handle_input(&dialog, event, &font, &ui_context, &heap, &scratch);
			}
			else
			{
				switch(event.type)
				{
					case ui::EventType::Button:
					{
						ui::Id id = event.button.id;
						if(id == import_button_id)
						{
							open_dialog(&dialog, &font, &ui_context, &heap);
							dialog.panel->unfocusable = false;
						}
						else if(id == export_button_id)
						{

						}
						break;
					}
				}
			}
		}
	}

	if(!ui_context.focused_container)
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

	// Draw the open file dialog.
	if(dialog.enabled)
	{
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, font_textures[0]);
		glBindSampler(0, nearest_repeat);

		Rect space;
		space.bottom_left = {0.0f, -250.0f};
		space.dimensions = {300.0f, 500.0f};
		ui::lay_out(dialog.panel, space);
		ui::draw(dialog.panel, &ui_context);
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
		ui::lay_out(main_menu, space);
		ui::draw(main_menu, &ui_context);
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

void pick_file(FilePickDialog* dialog, const char* name, ui::Context* context, Heap* heap, Stack* stack)
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

		close_dialog(dialog, context, heap);
		dialog->panel->unfocusable = true;
	}
}

void resize_viewport(int width, int height)
{
	viewport.width = width;
	viewport.height = height;

	ui_context.viewport.x = width;
	ui_context.viewport.y = height;

	const float fov = pi_over_2 * (2.0f / 3.0f);
	projection = perspective_projection_matrix(fov, width, height, near_plane, far_plane);
	sky_projection = perspective_projection_matrix(fov, width, height, 0.001f, 1.0f);
	screen_projection = orthographic_projection_matrix(width, height, -1.0f, 1.0f);
	glViewport(0, 0, width, height);
}

} // namespace video
