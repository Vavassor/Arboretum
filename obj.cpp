#include "obj.h"

#include "array2.h"
#include "assert.h"
#include "filesystem.h"
#include "jan.h"
#include "map.h"
#include "math_basics.h"
#include "memory.h"
#include "string_utilities.h"

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

    Float4* positions = nullptr;
    Float3* normals = nullptr;
    Float3* texcoords = nullptr;
    Label* materials = nullptr;
    MultiIndex* multi_indices = nullptr;
    Face* faces = nullptr;

    bool error_occurred = false;
    char* material_library = nullptr;
    int smoothing_group = 0;

    for(; stream_has_more(&stream) && !error_occurred; next_line(&stream))
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

            Float4 position;
            bool success_x = string_to_float(x, &position.x);
            bool success_y = string_to_float(y, &position.y);
            bool success_z = string_to_float(z, &position.z);
            position.w = 1.0f;
            bool success_w = true;
            if(w)
            {
                success_w = string_to_float(w, &position.w);
            }

            if(w)
            {
                STACK_DEALLOCATE(stack, w);
            }
            STACK_DEALLOCATE(stack, z);
            STACK_DEALLOCATE(stack, y);
            STACK_DEALLOCATE(stack, x);

            if(!success_x || !success_y || !success_z || !success_w)
            {
                error_occurred = true;
            }
            else
            {
                ARRAY_ADD(positions, position, heap);
            }
        }
        else if(strings_match(keyword, "vn"))
        {
            char* x = next_token(&stream, stack);
            char* y = next_token(&stream, stack);
            char* z = next_token(&stream, stack);

            Float3 normal;
            bool success_x = string_to_float(x, &normal.x);
            bool success_y = string_to_float(y, &normal.y);
            bool success_z = string_to_float(z, &normal.z);

            STACK_DEALLOCATE(stack, z);
            STACK_DEALLOCATE(stack, y);
            STACK_DEALLOCATE(stack, x);

            if(!success_x || !success_y || !success_z)
            {
                error_occurred = true;
            }
            else
            {
                ARRAY_ADD(normals, normal, heap);
            }
        }
        else if(strings_match(keyword, "vt"))
        {
            char* x = next_token(&stream, stack);
            char* y = next_token(&stream, stack);
            char* z = next_token(&stream, stack);

            Float3 texcoord;
            bool success_x = string_to_float(x, &texcoord.x);
            texcoord.y = 0.0f;
            texcoord.z = 0.0f;
            bool success_y = true;
            bool success_z = true;
            if(y)
            {
                success_y = string_to_float(y, &texcoord.y);
            }
            if(z)
            {
                success_z = string_to_float(z, &texcoord.z);
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

            if(!success_x || !success_y || !success_z)
            {
                error_occurred = true;
            }
            else
            {
                ARRAY_ADD(texcoords, texcoord, heap);
            }
        }
        else if(strings_match(keyword, "f"))
        {
            int base_index = array_count(multi_indices);
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
                bool success_position = string_to_int(position_index, &index.position);
                index.texcoord = index.position;
                index.normal = index.position;

                index.position = fix_index(index.position, array_count(positions));
                bool success_texcoord = true;
                bool success_normal = true;
                if(texcoord_index)
                {
                    success_texcoord = string_to_int(texcoord_index, &index.texcoord);
                    index.texcoord = fix_index(index.texcoord, array_count(texcoords));
                }
                if(normal_index)
                {
                    success_normal = string_to_int(normal_index, &index.normal);
                    index.normal = fix_index(index.normal, array_count(normals));
                }
                ARRAY_ADD(multi_indices, index, heap);

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

                if(!success_position || !success_texcoord || !success_normal)
                {
                    error_occurred = true;
                    break;
                }

                token = next_token(&stream, stack);
            }

            if(!error_occurred)
            {
                Face face;
                face.base_index = base_index;
                face.sides = indices_in_face;
                face.material_index = array_count(materials) - 1;
                ARRAY_ADD(faces, face, heap);
            }
        }
        else if(strings_match(keyword, "usemtl"))
        {
            char* name = next_token(&stream, stack);
            int size = string_size(name) + 1;
            Label material;
            material.name = HEAP_ALLOCATE(heap, char, size);
            copy_string(material.name, size, name);
            ARRAY_ADD(materials, material, heap);
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
            bool success_group = true;
            if(strings_match(token, "off"))
            {
                group = 0;
            }
            else
            {
                success_group = string_to_int(token, &group);
            }
            if(group >= 0)
            {
                smoothing_group = group;
            }
            STACK_DEALLOCATE(stack, token);

            if(!success_group)
            {
                error_occurred = true;
            }
        }

        STACK_DEALLOCATE(stack, keyword);
    }

    STACK_DEALLOCATE(stack, contents);

    error_occurred = error_occurred || array_count(positions) == 0;

    // Now that the file is finished processing, open the material library and
    // match up its contents to the stored material data.

    // TODO: material library

    // Fill the mesh with the completed data.
    if(!error_occurred)
    {
        jan::Mesh mesh;
        jan::create_mesh(&mesh);
        jan::Vertex** seen = STACK_ALLOCATE(stack, jan::Vertex*, array_count(positions));
        for(int i = 0; i < array_count(faces); i += 1)
        {
            Face obj_face = faces[i];
            jan::Vertex** vertices = STACK_ALLOCATE(stack, jan::Vertex*, obj_face.sides);
            for(int j = 0; j < obj_face.sides; j += 1)
            {
                MultiIndex index = multi_indices[obj_face.base_index + j];
                Float4 position = positions[index.position];
#if 0
                Float3 normal = normals[index.normal];
                Float3 texcoord = texcoords[index.texcoord];
#endif
                jan::Vertex* vertex;
                if(seen[index.position])
                {
                    vertex = seen[index.position];
                }
                else
                {
                    vertex = jan::add_vertex(&mesh, float4_extract_float3(position));
                    seen[index.position] = vertex;
                }
                vertices[j] = vertex;
            }
            jan::connect_disconnected_vertices_and_add_face(&mesh, vertices, obj_face.sides, stack);
            STACK_DEALLOCATE(stack, vertices);
        }
        jan::update_normals(&mesh);
        STACK_DEALLOCATE(stack, seen);
        *result = mesh;
    }

    // Cleanup

    for(int i = 0; i < array_count(materials); i += 1)
    {
        HEAP_DEALLOCATE(heap, materials[i].name);
    }

    ARRAY_DESTROY(positions, heap);
    ARRAY_DESTROY(normals, heap);
    ARRAY_DESTROY(texcoords, heap);
    ARRAY_DESTROY(materials, heap);
    ARRAY_DESTROY(multi_indices, heap);
    ARRAY_DESTROY(faces, heap);

    if(material_library)
    {
        HEAP_DEALLOCATE(heap, material_library);
    }

    return !error_occurred;
}

static bool vertex_attached_to_face(jan::Vertex* vertex)
{
    return vertex->any_edge && vertex->any_edge->any_link;
}

bool save_file(const char* path, jan::Mesh* mesh, Heap* heap)
{
    File* file = open_file(nullptr, FILE_OPEN_MODE_WRITE_TEMPORARY, heap);

    const int line_size = 128;
    char line[line_size];

    Map map;
    map_create(&map, mesh->vertices_count, heap);

    int index = 1;
    FOR_EACH_IN_POOL(jan::Vertex, vertex, mesh->vertex_pool)
    {
        if(!vertex_attached_to_face(vertex))
        {
            continue;
        }

        Float3 v = vertex->position;
        format_string(line, line_size, "v %.6f %.6f %.6f\n", v.x, v.y, v.z);
        write_file(file, line, string_size(line));

        void* value = reinterpret_cast<void*>(index);
        map_add(&map, vertex, value, heap);
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

        // @Incomplete: This outputs a filled face without any holes that might
        // be in it. .obj doesn't support holes, so the most reasonable way to
        // handle this would be to detect if the face has holes and, if it does,
        // split it into multiple faces.
        ASSERT(!face->first_border->next);

        jan::Link* first = face->first_border->first;
        jan::Link* link = first;
        do
        {
            void* value;
            map_get(&map, link->vertex, &value);
            uintptr_t index = reinterpret_cast<uintptr_t>(value);
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
