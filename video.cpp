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

const char* vertex_source_screen_pattern = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;

out vec3 surface_texcoord;
out vec4 surface_colour;

void main()
{
	vec4 surface_position = model_view_projection * vec4(position, 1.0);
    gl_Position = surface_position;
    surface_texcoord = vec3(surface_position.xy, surface_position.w);
	surface_colour = colour;
}
)";

const char* fragment_source_screen_pattern = R"(
#version 330

uniform sampler2D texture;
uniform vec2 pattern_scale;
uniform vec2 viewport_dimensions;
uniform vec2 texture_dimensions;

layout(location = 0) out vec4 output_colour;

in vec3 surface_texcoord;
in vec4 surface_colour;

void main()
{
	vec2 screen_texcoord = surface_texcoord.xy / surface_texcoord.z;
	screen_texcoord *= viewport_dimensions;
	screen_texcoord /= texture_dimensions;
	screen_texcoord *= pattern_scale;
	output_colour = surface_colour * texture2D(texture, screen_texcoord);
}
)";

struct Object
{
	Matrix4 model;
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

	object->model = matrix4_identity;
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

static void object_set_matrices(Object* object, Matrix4 view, Matrix4 projection)
{
	Matrix4 model_view = view * object->model;
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
	GLuint linear_repeat;

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
		GLint texture;
		GLint viewport_dimensions;
		GLint texture_dimensions;
		GLint pattern_scale;
	} shader_screen_pattern;

	struct
	{
		GLuint program;
		GLint model_view_projection;
	} shader_halo;

	Object sky;
	Object* objects;
	int objects_count;

	jan::Mesh* meshes;
	int meshes_count;

	int selected_object_index;
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
	GLuint hatch_pattern;

	ui::Item* main_menu;
	ui::Context ui_context;
	ui::Id import_button_id;
	ui::Id export_button_id;
}

static Object* add_object(Heap* heap)
{
	objects = HEAP_REALLOCATE(heap, Object, objects, objects_count + 1);

	Object* object = &objects[objects_count];
	objects_count += 1;
	object_create(object);

	return object;
}

static jan::Mesh* add_mesh(Heap* heap)
{
	meshes = HEAP_REALLOCATE(heap, jan::Mesh, meshes, meshes_count + 1);

	jan::Mesh* mesh = &meshes[meshes_count];
	meshes_count += 1;
	jan::create_mesh(mesh);

	return mesh;
}

static void select_mesh(jan::Mesh* mesh)
{
	jan::destroy_selection(&selection);

	jan::create_selection(&selection, &heap);
	selection.type = jan::Selection::Type::Face;

	FOR_N(i, meshes_count)
	{
		if(&meshes[i] == mesh)
		{
			selected_object_index = i;
			break;
		}
	}
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

		glGenSamplers(1, &linear_repeat);
		glSamplerParameteri(linear_repeat, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(linear_repeat, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(linear_repeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(linear_repeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

	// Screen Pattern Shader
	shader_screen_pattern.program = load_shader_program(vertex_source_screen_pattern, fragment_source_screen_pattern, &scratch);
	if(shader_screen_pattern.program == 0)
	{
		LOG_ERROR("The screen pattern shader failed to load.");
		return false;
	}
	{
		GLuint program = shader_screen_pattern.program;
		shader_screen_pattern.model_view_projection = glGetUniformLocation(program, "model_view_projection");
		shader_screen_pattern.texture = glGetUniformLocation(program, "texture");
		shader_screen_pattern.viewport_dimensions = glGetUniformLocation(program, "viewport_dimensions");
		shader_screen_pattern.texture_dimensions = glGetUniformLocation(program, "texture_dimensions");
		shader_screen_pattern.pattern_scale = glGetUniformLocation(program, "pattern_scale");

		const Vector2 pattern_scale = {2.5f, 2.5f};

		glUseProgram(shader_screen_pattern.program);
		glUniform1i(shader_screen_pattern.texture, 0);
		glUniform2fv(shader_screen_pattern.pattern_scale, 1, &pattern_scale[0]);
	}

	// Sky
	object_create(&sky);
	object_generate_sky(&sky, &scratch);

	// Dodecahedron
	{
		Object* dodecahedron = add_object(&heap);
		jan::Mesh* mesh = add_mesh(&heap);

		jan::make_a_weird_face(mesh);

		jan::Selection selection = jan::select_all(mesh, &heap);
		jan::extrude(mesh, &selection, 0.6f, &scratch);
		jan::destroy_selection(&selection);

		jan::colour_all_faces(mesh, vector3_cyan);

		VertexPNC* vertices;
		int vertices_count;
		u16* indices;
		int indices_count;
		jan::triangulate(mesh, &heap, &vertices, &vertices_count, &indices, &indices_count);
		object_set_surface(dodecahedron, vertices, vertices_count, indices, indices_count);
		HEAP_DEALLOCATE(&heap, vertices);
		HEAP_DEALLOCATE(&heap, indices);

		obj::save_file("weird.obj", mesh, &heap);

		Quaternion orientation = axis_angle_rotation(vector3_unit_x, pi / 4.0f);
		dodecahedron->model = compose_transform({2.0f, 0.0f, 0.0f}, orientation, vector3_one);

		select_mesh(mesh);
	}

	// Test Model
	{
		jan::Mesh* mesh = add_mesh(&heap);

		obj::load_file("test.obj", mesh, &heap, &scratch);
		jan::colour_all_faces(mesh, vector3_yellow);

		Object* test_model = add_object(&heap);
		test_model->model = matrix4_identity;

		VertexPNC* vertices;
		int vertices_count;
		u16* indices;
		int indices_count;
		jan::triangulate(mesh, &heap, &vertices, &vertices_count, &indices, &indices_count);
		object_set_surface(test_model, vertices, vertices_count, indices, indices_count);
		HEAP_DEALLOCATE(&heap, vertices);
		HEAP_DEALLOCATE(&heap, indices);
	}

	// Camera
	camera.position = {-4.0f, -4.0f, 2.0f};
	camera.target = vector3_zero;

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

	// Hatch pattern texture
	{
		const char* path = "polka_dot.png";
		Bitmap bitmap;
		bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
		hatch_pattern = upload_bitmap(&bitmap);
		stbi_image_free(bitmap.pixels);

		glUseProgram(shader_screen_pattern.program);
		glUniform2f(shader_screen_pattern.texture_dimensions, bitmap.width, bitmap.height);
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
		glDeleteSamplers(1, &linear_repeat);

		glDeleteProgram(shader_vertex_colour.program);
		glDeleteProgram(shader_texture_only.program);
		glDeleteProgram(shader_font.program);
		glDeleteProgram(shader_lit.program);
		glDeleteProgram(shader_halo.program);
		glDeleteProgram(shader_screen_pattern.program);

		FOR_N(i, 1)
		{
			glDeleteTextures(1, &font_textures[i]);
		}
		glDeleteTextures(1, &hatch_pattern);

		object_destroy(&sky);
		FOR_N(i, objects_count)
		{
			object_destroy(&objects[i]);
		}
		SAFE_HEAP_DEALLOCATE(&heap, objects);

		FOR_N(i, meshes_count)
		{
			jan::destroy_mesh(&meshes[i]);
		}
		SAFE_HEAP_DEALLOCATE(&heap, meshes);

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

	Vector3 near = {point.x, point.y, 0.0f};
	Vector3 far = {point.x, point.y, 1.0f};

	Vector3 start = inverse * near;
	Vector3 end = inverse * far;

	Ray result;
	result.origin = start;
	result.direction = normalise(end - start);
	return result;
}

static Ray transform_ray(Ray ray, Matrix4 transform)
{
	Ray result;
	result.origin = transform * ray.origin;
	result.direction = normalise(transform * (ray.direction + ray.origin) - result.origin);
	return result;
}

static void draw_move_tool(bool silhouetted)
{
	const float shaft_length = 2.0f * sqrt(3.0f);
	const float shaft_radius = 0.125f;
	const float head_height = sqrt(3.0f) / 2.0f;
	const float head_radius = 0.5f;
	const float quadrant_length = 1.0f;
	const float quadrant_radius = 0.0625f;
	const float ball_radius = 0.4f;

	const Vector3 xy_quadrant = {quadrant_length, quadrant_length, 0.0f};
	const Vector3 yz_quadrant = {0.0f, quadrant_length, quadrant_length};
	const Vector3 xz_quadrant = {quadrant_length, 0.0f, quadrant_length};

	Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
	Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
	Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};
	Vector4 x_axis_shadow_colour = {0.7f, 0.0314f, 0.0314f, 1.0f};
	Vector4 y_axis_shadow_colour = {0.3569f, 0.7f, 0.0f, 1.0f};
	Vector4 z_axis_shadow_colour = {0.0863f, 0.0314f, 0.7f, 1.0f};

	const Vector4 ball_colour = {0.9f, 0.9f, 0.9f, 1.0f};

	Vector3 shaft_axis_x = {shaft_length, 0.0f, 0.0f};
	Vector3 shaft_axis_y = {0.0f, shaft_length, 0.0f};
	Vector3 shaft_axis_z = {0.0f, 0.0f, shaft_length};

	Vector3 head_axis_x = {head_height, 0.0f, 0.0f};
	Vector3 head_axis_y = {0.0f, head_height, 0.0f};
	Vector3 head_axis_z = {0.0f, 0.0f, head_height};

	Vector3 quadrant_x = {quadrant_length, 0.0, 0.0f};
	Vector3 quadrant_y = {0.0, quadrant_length, 0.0f};
	Vector3 quadrant_z = {0.0, 0.0f, quadrant_length};

	immediate::add_cone(shaft_axis_x, head_axis_x, head_radius, x_axis_colour, x_axis_shadow_colour);
	immediate::add_cylinder(vector3_zero, shaft_axis_x, shaft_radius, x_axis_colour);
	immediate::add_cylinder(quadrant_x, xy_quadrant, quadrant_radius, x_axis_colour);
	immediate::add_cylinder(quadrant_x, xz_quadrant, quadrant_radius, x_axis_colour);

	immediate::add_cone(shaft_axis_y, head_axis_y, head_radius, y_axis_colour, y_axis_shadow_colour);
	immediate::add_cylinder(vector3_zero, shaft_axis_y, shaft_radius, y_axis_colour);
	immediate::add_cylinder(quadrant_y, xy_quadrant, quadrant_radius, y_axis_colour);
	immediate::add_cylinder(quadrant_y, yz_quadrant, quadrant_radius, y_axis_colour);

	immediate::add_cone(shaft_axis_z, head_axis_z, head_radius, z_axis_colour, z_axis_shadow_colour);
	immediate::add_cylinder(vector3_zero, shaft_axis_z, shaft_radius, z_axis_colour);
	immediate::add_cylinder(quadrant_z, xz_quadrant, quadrant_radius, z_axis_colour);
	immediate::add_cylinder(quadrant_z, yz_quadrant, quadrant_radius, z_axis_colour);

	immediate::add_sphere(vector3_zero, ball_radius, ball_colour);

	immediate::draw();
}

void draw_rotate_tool(bool silhouetted)
{
	const float radius = 2.0f;
	const float width = 0.3f;

	Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
	Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
	Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};

	immediate::add_arc(vector3_zero, vector3_unit_x, pi_over_2, -pi_over_2, radius, width, x_axis_colour);
	immediate::add_arc(vector3_zero, -vector3_unit_x, pi_over_2, pi, radius, width, x_axis_colour);

	immediate::add_arc(vector3_zero, vector3_unit_y, pi_over_2, -pi_over_2, radius, width, y_axis_colour);
	immediate::add_arc(vector3_zero, -vector3_unit_y, pi_over_2, pi, radius, width, y_axis_colour);

	immediate::add_arc(vector3_zero, vector3_unit_z, pi_over_2, pi_over_2, radius, width, z_axis_colour);
	immediate::add_arc(vector3_zero, -vector3_unit_z, pi_over_2, 0.0f, radius, width, z_axis_colour);

	immediate::draw();
}

void draw_scale_tool(bool silhouetted)
{
	const float shaft_length = 2.0f * sqrt(3.0f);
	const float shaft_radius = 0.125f;
	const float knob_extent = 0.3333f;
	const float brace = 0.6666f * shaft_length;
	const float brace_radius = shaft_radius / 2.0f;

	Vector3 knob_extents = {knob_extent, knob_extent, knob_extent};

	Vector3 shaft_axis_x = {shaft_length, 0.0f, 0.0f};
	Vector3 shaft_axis_y = {0.0f, shaft_length, 0.0f};
	Vector3 shaft_axis_z = {0.0f, 0.0f, shaft_length};

	Vector3 brace_x = {brace, 0.0f, 0.0f};
	Vector3 brace_y = {0.0f, brace, 0.0f};
	Vector3 brace_z = {0.0f, 0.0f, brace};
	Vector3 brace_xy = (brace_x + brace_y) / 2.0f;
	Vector3 brace_yz = (brace_y + brace_z) / 2.0f;
	Vector3 brace_xz = (brace_x + brace_z) / 2.0f;

	const Vector4 origin_colour = {0.9f, 0.9f, 0.9f, 1.0f};
	const Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
	const Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
	const Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};

	immediate::add_cylinder(vector3_zero, shaft_axis_x, shaft_radius, x_axis_colour);
	immediate::add_box(shaft_axis_x, knob_extents, x_axis_colour);
	immediate::add_cylinder(brace_x, brace_xy, brace_radius, x_axis_colour);
	immediate::add_cylinder(brace_x, brace_xz, brace_radius, x_axis_colour);

	immediate::add_cylinder(vector3_zero, shaft_axis_y, shaft_radius, y_axis_colour);
	immediate::add_box(shaft_axis_y, knob_extents, y_axis_colour);
	immediate::add_cylinder(brace_y, brace_xy, brace_radius, y_axis_colour);
	immediate::add_cylinder(brace_y, brace_yz, brace_radius, y_axis_colour);

	immediate::add_cylinder(vector3_zero, shaft_axis_z, shaft_radius, z_axis_colour);
	immediate::add_box(shaft_axis_z, knob_extents, z_axis_colour);
	immediate::add_cylinder(brace_z, brace_xz, brace_radius, z_axis_colour);
	immediate::add_cylinder(brace_z, brace_yz, brace_radius, z_axis_colour);

	immediate::add_box(vector3_zero, knob_extents, origin_colour);

	immediate::draw();
}

static void update_face_mode()
{
	ASSERT(selected_object_index != -1);
	ASSERT(selected_object_index >= 0 && selected_object_index < objects_count);
	ASSERT(selected_object_index >= 0 && selected_object_index < meshes_count);

	Object* object = &objects[selected_object_index];
	jan::Mesh* mesh = &meshes[selected_object_index];

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
		move_faces(mesh, &selection, move);
	}

	// Cast a ray from the mouse to the test model.
	{
		jan::colour_all_faces(mesh, vector3_yellow);
		jan::colour_selection(mesh, &selection, vector3_cyan);

		Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
		Ray ray = ray_from_viewport_point(mouse.position, viewport.width, viewport.height, view, projection, false);
		ray = transform_ray(ray, inverse_transform(object->model));
		jan::Face* face = first_face_hit_by_ray(mesh, ray);
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
		object_update_mesh(object, mesh, &heap);
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
		update_face_mode();
	}

	// Update matrices.
	{
		Vector3 direction = normalise(camera.target - camera.position);
		Matrix4 view = look_at_matrix(vector3_zero, direction, vector3_unit_z);
		object_set_matrices(&sky, view, sky_projection);

		view = look_at_matrix(camera.position, camera.target, vector3_unit_z);

		FOR_N(i, objects_count)
		{
			object_set_matrices(&objects[i], view, projection);
		}

		immediate::set_matrices(view, projection);

		// Update light parameters.

		Vector3 light_direction = {0.7f, 0.4f, -1.0f};
		light_direction = normalise(-(view * light_direction));

		glUseProgram(shader_lit.program);
		glUniform3fv(shader_lit.light_direction, 1, &light_direction[0]);
	}

	glUseProgram(shader_lit.program);

	// Draw all unselected models.
	FOR_N(i, objects_count)
	{
		if(i == selected_object_index)
		{
			continue;
		}
		Object object = objects[i];
		glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
		glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, object.normal.elements);
		glBindVertexArray(object.vertex_array);
		glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);
	}

	// Draw the selected model.
	{
		Object object = objects[selected_object_index];

		glEnable(GL_STENCIL_TEST);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		glStencilMask(0xff);
		glClear(GL_STENCIL_BUFFER_BIT);

		glStencilFunc(GL_ALWAYS, 1, 0xff);

		glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
		glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, object.normal.elements);
		glBindVertexArray(object.vertex_array);
		glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);

		glUseProgram(shader_halo.program);

		// Draw the selected model's halo.
		glStencilFunc(GL_NOTEQUAL, 1, 0xff);
		glStencilMask(0x00);

		glLineWidth(3.0f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glUniformMatrix4fv(shader_halo.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
		glBindVertexArray(object.vertex_array);
		glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);

		glLineWidth(1.0f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_STENCIL_TEST);
	}

	glUseProgram(shader_lit.program);

	// rotate tool
	{
		Vector3 center = {-5.0f, 0.0f, 0.0f};
		Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
		immediate::set_matrices(view, projection);

		draw_rotate_tool(false);
	}

	// move tool
	{
		Vector3 position = {5.0f, 0.0f, 0.0f};
		float d = distance(camera.position, position);
		d *= 0.05f;
		Vector3 scale = {d, d, d};

		Matrix4 model = compose_transform(position, quaternion_identity, scale);
		Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
		immediate::set_matrices(view * model, projection);

		glDisable(GL_DEPTH_TEST);
		immediate::set_shader(shader_screen_pattern.program);
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, hatch_pattern);
		glBindSampler(0, linear_repeat);
		draw_move_tool(true);

		glEnable(GL_DEPTH_TEST);
		immediate::set_shader(shader_vertex_colour.program);
		draw_move_tool(false);
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

	// Draw the little axes in the corner.
	{
		const int scale = 40;
		const int padding = 10;

		const Vector3 x_axis = {sqrt(3.0f) / 2.0f, 0.0f, 0.0f};
		const Vector3 y_axis = {0.0f, sqrt(3.0f) / 2.0f, 0.0f};
		const Vector3 z_axis = {0.0f, 0.0f, sqrt(3.0f) / 2.0f};

		const Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
		const Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
		const Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};
		const Vector4 x_axis_shadow_colour = {0.7f, 0.0314f, 0.0314f, 1.0f};
		const Vector4 y_axis_shadow_colour = {0.3569f, 0.7f, 0.0f, 1.0f};
		const Vector4 z_axis_shadow_colour = {0.0863f, 0.0314f, 0.7f, 1.0f};

		int corner_x = viewport.width - scale - padding;
		int corner_y = padding;
		glViewport(corner_x, corner_y, scale, scale);
		glClear(GL_DEPTH_BUFFER_BIT);

		const float across = 3.0f * sqrt(3.0f);
		const float extent = across / 2.0f;

		Vector3 direction = normalise(camera.target - camera.position);
		Matrix4 view = look_at_matrix(vector3_zero, direction, vector3_unit_z);
		Matrix4 axis_projection = orthographic_projection_matrix(across, across, -extent, extent);
		immediate::set_matrices(view, axis_projection);

		immediate::add_cone(2.0f * x_axis, x_axis, 0.5f, x_axis_colour, x_axis_shadow_colour);
		immediate::add_cylinder(vector3_zero, 2.0f * x_axis, 0.125f, x_axis_colour);
		immediate::add_cone(2.0f * y_axis, y_axis, 0.5f, y_axis_colour, y_axis_shadow_colour);
		immediate::add_cylinder(vector3_zero, 2.0f * y_axis, 0.125f, y_axis_colour);
		immediate::add_cone(2.0f * z_axis, z_axis, 0.5f, z_axis_colour, z_axis_shadow_colour);
		immediate::add_cylinder(vector3_zero, 2.0f * z_axis, 0.125f, z_axis_colour);
		immediate::draw();
	}

	// Draw the screen-space UI.
	glViewport(0, 0, viewport.width, viewport.height);
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

	// Draw the main menu.
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
	jan::Mesh* mesh = add_mesh(heap);
	bool loaded = obj::load_file(path, mesh, heap, stack);
	HEAP_DEALLOCATE(heap, path);

	if(!loaded)
	{
		// TODO: Report this to the user, not the log!
		LOG_DEBUG("Failed to load the file %s as an .obj.", name);
	}
	else
	{
		Object* imported_model = add_object(heap);
		imported_model->model = compose_transform({-2.0f, 0.0f, 0.0f}, quaternion_identity, vector3_one);

		jan::colour_all_faces(mesh, vector3_magenta);
		VertexPNC* vertices;
		int vertices_count;
		u16* indices;
		int indices_count;
		jan::triangulate(mesh, heap, &vertices, &vertices_count, &indices, &indices_count);
		object_set_surface(imported_model, vertices, vertices_count, indices, indices_count);
		HEAP_DEALLOCATE(heap, vertices);
		HEAP_DEALLOCATE(heap, indices);

		close_dialog(dialog, context, heap);
		dialog->panel->unfocusable = true;
	}
}

void resize_viewport(int width, int height, double dots_per_millimeter)
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

	// Update any shaders that use the viewport dimensions.
	glUseProgram(shader_screen_pattern.program);
	glUniform2f(shader_screen_pattern.viewport_dimensions, viewport.width, viewport.height);
}

} // namespace video
