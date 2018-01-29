#include "obj.h"

#include "assert.h"
#include "jan.h"
#include "filesystem.h"
#include "memory.h"
#include "logging.h"
#include "string_utilities.h"
#include "array.h"
#include "math_basics.h"

namespace obj {

struct Stream
{
	const char* buffer;
};

static bool is_whitespace(char c)
{
	return c == ' ' || c - 9 <= 5;
}

static bool is_space_or_tab(char c)
{
	return c == ' ' || c == '\t';
}

static bool is_newline(char c)
{
	return c == '\n' || c == '\r';
}

static bool stream_has_more(Stream* stream)
{
	return *stream->buffer;
}

static void skip_spacing(Stream* stream)
{
	const char* s;
	for(s = stream->buffer; is_space_or_tab(*s); s += 1);
	stream->buffer = s;
}

static void next_line(Stream* stream)
{
	const char* s;
	for(s = stream->buffer; *s && !is_newline(*s); s += 1);
	if(*s)
	{
		s += 1;
	}
	stream->buffer = s;
}

static char* next_token(Stream* stream, Stack* stack)
{
	skip_spacing(stream);

	const char* first = stream->buffer;
	const char* s;
	for(s = first; *s && !is_whitespace(*s); ++s);
	int token_size = s - first;

	if(token_size == 0)
	{
		return nullptr;
	}

	char* token = STACK_ALLOCATE(stack, char, token_size + 1);
	copy_string(token, token_size + 1, stream->buffer);
	stream->buffer += token_size;

	return token;
}

static char* next_index(Stream* stream, Stack* stack)
{
	const char* first = stream->buffer;
	const char* s;
	bool slash_found = false;
	for(s = first; *s && !is_whitespace(*s); ++s)
	{
		if(*s == '/')
		{
			slash_found = true;
			break;
		}
	}
	int index_size = s - first;

	if(index_size == 0)
	{
		if(*stream->buffer && slash_found)
		{
			// Skip the slash.
			stream->buffer += 1;
		}
		return nullptr;
	}

	char* index = STACK_ALLOCATE(stack, char, index_size + 1);
	copy_string(index, index_size + 1, stream->buffer);
	stream->buffer += index_size;

	return index;
}

static int fix_index(int index, int total)
{
	if(index < 0)
	{
		// Negative indices are relative to the end of the array so far.
		index += total;
	}
	else
	{
		// Make the index zero-based.
		index -= 1;
	}
	return index;
}

struct Label
{
	char* name;
};

struct MultiIndex
{
	int position;
	int texcoord;
	int normal;
};

struct Face
{
	int base_index;
	int sides;
	int material_index;
};

DEFINE_ARRAY(Vector4);
DEFINE_ARRAY(Vector3);
DEFINE_ARRAY(Label);
DEFINE_ARRAY(MultiIndex);
DEFINE_ARRAY(Face);

bool load_file(const char* path, jan::Mesh* result, Heap* heap, Stack* stack)
{
	void* contents;
	u64 bytes;
	bool contents_loaded = load_whole_file(path, &contents, &bytes, stack);
	if(!contents_loaded)
	{
		return false;
	}

	Stream stream;
	stream.buffer = static_cast<char*>(contents);

	ArrayVector4 positions;
	create(&positions, heap);

	ArrayVector3 normals;
	create(&normals, heap);

	ArrayVector3 texcoords;
	create(&texcoords, heap);

	ArrayLabel materials;
	create(&materials, heap);

	ArrayMultiIndex multi_indices;
	create(&multi_indices, heap);

	ArrayFace faces;
	create(&faces, heap);

	bool error_occurred = false;
	char* material_library = nullptr;
	int smoothing_group = 0;

	for(; stream_has_more(&stream); next_line(&stream))
	{
		char* keyword = next_token(&stream, stack);

		if(!keyword)
		{
			error_occurred = true;
			break;
		}
		else if(strings_match(keyword, "v"))
		{
			char* x = next_token(&stream, stack);
			char* y = next_token(&stream, stack);
			char* z = next_token(&stream, stack);
			char* w = next_token(&stream, stack);

			Vector4 position;
			position.x = string_to_float(x);
			position.y = string_to_float(y);
			position.z = string_to_float(z);
			position.w = 1.0f;
			if(w)
			{
				position.w = string_to_float(w);
			}

			if(w)
			{
				STACK_DEALLOCATE(stack, w);
			}
			STACK_DEALLOCATE(stack, z);
			STACK_DEALLOCATE(stack, y);
			STACK_DEALLOCATE(stack, x);

			add_and_expand(&positions, position);
		}
		else if(strings_match(keyword, "vn"))
		{
			char* x = next_token(&stream, stack);
			char* y = next_token(&stream, stack);
			char* z = next_token(&stream, stack);

			Vector3 normal;
			normal.x = string_to_float(x);
			normal.y = string_to_float(y);
			normal.z = string_to_float(z);

			STACK_DEALLOCATE(stack, z);
			STACK_DEALLOCATE(stack, y);
			STACK_DEALLOCATE(stack, x);

			add_and_expand(&normals, normal);
		}
		else if(strings_match(keyword, "vt"))
		{
			char* x = next_token(&stream, stack);
			char* y = next_token(&stream, stack);
			char* z = next_token(&stream, stack);

			Vector3 texcoord;
			texcoord.x = string_to_float(x);
			texcoord.y = 0.0f;
			texcoord.z = 0.0f;
			if(y)
			{
				texcoord.y = string_to_float(y);
			}
			if(z)
			{
				texcoord.z = string_to_float(z);
			}

			if(z)
			{
				STACK_DEALLOCATE(stack, z);
			}
			if(y)
			{
				STACK_DEALLOCATE(stack, y);
			}
			STACK_DEALLOCATE(stack, x);

			add_and_expand(&texcoords, texcoord);
		}
		else if(strings_match(keyword, "f"))
		{
			int base_index = multi_indices.count;
			int indices_in_face = 0;

			char* token = next_token(&stream, stack);
			while(token)
			{
				Stream token_stream;
				token_stream.buffer = token;
				char* position_index = next_index(&token_stream, stack);
				char* texcoord_index = next_index(&token_stream, stack);
				char* normal_index = next_index(&token_stream, stack);

				MultiIndex index;
				index.position = string_to_int(position_index);
				index.texcoord = index.position;
				index.normal = index.position;

				index.position = fix_index(index.position, positions.count);
				if(texcoord_index)
				{
					index.texcoord = string_to_int(texcoord_index);
					index.texcoord = fix_index(index.texcoord, texcoords.count);
				}
				if(normal_index)
				{
					index.normal = string_to_int(normal_index);
					index.normal = fix_index(index.normal, normals.count);
				}
				add_and_expand(&multi_indices, index);

				if(normal_index)
				{
					STACK_DEALLOCATE(stack, normal_index);
				}
				if(texcoord_index)
				{
					STACK_DEALLOCATE(stack, texcoord_index);
				}
				STACK_DEALLOCATE(stack, position_index);

				indices_in_face += 1;
				STACK_DEALLOCATE(stack, token);
				token = next_token(&stream, stack);
			}

			Face face;
			face.base_index = base_index;
			face.sides = indices_in_face;
			face.material_index = materials.count - 1;
			add_and_expand(&faces, face);
		}
		else if(strings_match(keyword, "usemtl"))
		{
			char* name = next_token(&stream, stack);
			int size = string_size(name) + 1;
			Label material;
			material.name = HEAP_ALLOCATE(heap, char, size);
			copy_string(material.name, size, name);
			add_and_expand(&materials, material);
		}
		else if(strings_match(keyword, "mtllib"))
		{
			char* library = next_token(&stream, stack);
			int size = string_size(library) + 1;
			material_library = HEAP_ALLOCATE(heap, char, size);
			copy_string(material_library, size, library);
			STACK_DEALLOCATE(stack, library);
		}
		else if(strings_match(keyword, "s"))
		{
			char* token = next_token(&stream, stack);
			int group;
			if(strings_match(token, "off"))
			{
				group = 0;
			}
			else
			{
				group = string_to_int(token);
			}
			if(group >= 0)
			{
				smoothing_group = group;
			}
			STACK_DEALLOCATE(stack, token);
		}

		STACK_DEALLOCATE(stack, keyword);
	}

	STACK_DEALLOCATE(stack, contents);

	error_occurred = error_occurred || positions.count == 0;

	// Now that the file is finished processing, open the material library and
	// match up its contents to the stored material data.

	// TODO: material library

	// Fill the mesh with the completed data.
	if(!error_occurred)
	{
		jan::Mesh mesh;
		jan::create_mesh(&mesh);
		jan::Vertex** seen = STACK_ALLOCATE(stack, jan::Vertex*, positions.count);
		for(int i = 0; i < faces.count; ++i)
		{
			Face obj_face = faces[i];
			jan::Vertex* vertices[obj_face.sides];
			for(int j = 0; j < obj_face.sides; ++j)
			{
				MultiIndex index = multi_indices[obj_face.base_index + j];
				Vector4 position = positions[index.position];
#if 0
				Vector3 normal = normals[index.normal];
				Vector3 texcoord = texcoords[index.texcoord];
#endif
				jan::Vertex* vertex;
				if(seen[index.position])
				{
					vertex = seen[index.position];
				}
				else
				{
					vertex = jan::add_vertex(&mesh, extract_vector3(position));
					seen[index.position] = vertex;
				}
				vertices[j] = vertex;
			}
			jan::connect_disconnected_vertices_and_add_face(&mesh, vertices, obj_face.sides);
		}
		jan::update_normals(&mesh);
		STACK_DEALLOCATE(stack, seen);
		*result = mesh;
	}

	// Cleanup

	for(int i = 0; i < materials.count; i += 1)
	{
		HEAP_DEALLOCATE(heap, materials[i].name);
	}

	destroy(&positions);
	destroy(&normals);
	destroy(&texcoords);
	destroy(&materials);
	destroy(&multi_indices);
	destroy(&faces);

	if(material_library)
	{
		HEAP_DEALLOCATE(heap, material_library);
	}

	return !error_occurred;
}

struct VertexMap
{
	struct Pair
	{
		jan::Vertex* key;
		int value;
	};

	Pair* pairs;
	int count;
	int cap;
};

static void map_create(VertexMap* map, int cap, Heap* heap)
{
	map->pairs = HEAP_ALLOCATE(heap, VertexMap::Pair, cap);
	map->count = 0;
	map->cap = cap;
}

static void map_destroy(VertexMap* map, Heap* heap)
{
	HEAP_DEALLOCATE(heap, map->pairs);
}

static upointer hash_pointer(void* pointer)
{
	upointer shift = log2(static_cast<double>(1 + sizeof(pointer)));
	return reinterpret_cast<upointer>(pointer) >> shift;
}

static int map_find(VertexMap* map, jan::Vertex* key)
{
	int first = hash_pointer(key) % map->cap;
	int index = first;
	while(map->pairs[index].value && map->pairs[index].key != key)
	{
		index = (index + 1) % map->cap;
		if(index == first)
		{
			return 0;
		}
	}
	return map->pairs[index].value;
}

static void map_add(VertexMap* map, jan::Vertex* key, int value)
{
	ASSERT(map->count < map->cap);
	int index = hash_pointer(key) % map->cap;
	while(map->pairs[index].value)
	{
		index = (index + 1) % map->cap;
	}
	map->pairs[index].key = key;
	map->pairs[index].value = value;
}

static bool vertex_attached_to_face(jan::Vertex* vertex)
{
	return vertex->any_edge && vertex->any_edge->any_link;
}

bool save_file(const char* path, jan::Mesh* mesh, Heap* heap)
{
	File* file = open_file(nullptr, FileOpenMode::Write_Temporary, heap);

	const int line_size = 128;
	char line[line_size];

	VertexMap map;
	map_create(&map, mesh->vertices_count, heap);

	int index = 1;
	FOR_EACH_IN_POOL(jan::Vertex, vertex, mesh->vertex_pool)
	{
		if(!vertex_attached_to_face(vertex))
		{
			continue;
		}

		Vector3 v = vertex->position;
		format_string(line, line_size, "v %.6f %.6f %.6f\n", v.x, v.y, v.z);
		write_file(file, line, string_size(line));

		map_add(&map, vertex, index);
		index += 1;
	}

	const char* usemtl = "usemtl None\n";
	write_file(file, usemtl, string_size(usemtl));

	const char* s = "s off\n";
	write_file(file, s, string_size(s));

	FOR_EACH_IN_POOL(jan::Face, face, mesh->face_pool)
	{
		int i = 0;
		int line_left = line_size;
		int copied = copy_string(&line[i], line_left, "f");
		i += copied;
		line_left -= copied;

		jan::Link* first = face->link;
		jan::Link* link = first;
		do
		{
			int index = map_find(&map, link->vertex);
			char text[22];
			text[0] = ' ';
			int_to_string(text + 1, 21, index);
			copied = copy_string(&line[i], line_left, text);
			i += copied;
			line_left -= copied;

			link = link->next;
		} while(link != first);

		copy_string(&line[i], line_left, "\n");
		write_file(file, line, string_size(line));
	}

	map_destroy(&map, heap);

	make_file_permanent(file, path);
	close_file(file);

	return true;
}

} // namespace obj
