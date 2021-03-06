#include "obj.h"

#include "array2.h"
#include "ascii.h"
#include "assert.h"
#include "filesystem.h"
#include "jan.h"
#include "map.h"
#include "math_basics.h"
#include "memory.h"
#include "string_utilities.h"

typedef struct Stream
{
    const char* buffer;
} Stream;

static bool stream_has_more(Stream* stream)
{
    return *stream->buffer;
}

static void skip_spacing(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; ascii_is_space_or_tab(*s); s += 1);
    stream->buffer = s;
}

static void next_line(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; *s && !ascii_is_newline(*s); s += 1);
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
    for(s = first; *s && !ascii_is_whitespace(*s); ++s);
    int token_size = (int) (s - first);

    if(token_size == 0)
    {
        return NULL;
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
    for(s = first; *s && !ascii_is_whitespace(*s); ++s)
    {
        if(*s == '/')
        {
            slash_found = true;
            break;
        }
    }
    int index_size = (int) (s - first);

    if(index_size == 0)
    {
        if(*stream->buffer && slash_found)
        {
            // Skip the slash.
            stream->buffer += 1;
        }
        return NULL;
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

typedef struct Label
{
    char* name;
} Label;

typedef struct MultiIndex
{
    int position;
    int texcoord;
    int normal;
} MultiIndex;

typedef struct Face
{
    int base_index;
    int sides;
    int material_index;
} Face;

bool obj_load_file(const char* path, JanMesh* result, Heap* heap, Stack* stack)
{
    WholeFile whole_file = load_whole_file(path, stack);
    if(!whole_file.loaded)
    {
        return false;
    }

    Stream stream;
    stream.buffer = (char*) whole_file.contents;

    Float4* positions = NULL;
    Float3* normals = NULL;
    Float3* texcoords = NULL;
    Label* materials = NULL;
    MultiIndex* multi_indices = NULL;
    Face* faces = NULL;

    bool error_occurred = false;
    char* material_library = NULL;
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
            MaybeFloat maybe_x = string_to_float(x);
            MaybeFloat maybe_y = string_to_float(y);
            MaybeFloat maybe_z = string_to_float(z);
            position.x = maybe_x.value;
            position.y = maybe_y.value;
            position.z = maybe_z.value;
            position.w = 1.0f;
            bool success_w = true;
            if(w)
            {
                MaybeFloat maybe_w = string_to_float(w);
                success_w = maybe_w.valid;
                position.w = maybe_w.value;
            }

            if(w)
            {
                STACK_DEALLOCATE(stack, w);
            }
            STACK_DEALLOCATE(stack, z);
            STACK_DEALLOCATE(stack, y);
            STACK_DEALLOCATE(stack, x);

            if(!maybe_x.valid || !maybe_y.valid || !maybe_z.valid || !success_w)
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
            MaybeFloat maybe_x = string_to_float(x);
            MaybeFloat maybe_y = string_to_float(y);
            MaybeFloat maybe_z = string_to_float(z);
            normal.x = maybe_x.value;
            normal.y = maybe_y.value;
            normal.z = maybe_z.value;

            STACK_DEALLOCATE(stack, z);
            STACK_DEALLOCATE(stack, y);
            STACK_DEALLOCATE(stack, x);

            if(!maybe_x.valid || !maybe_y.valid || !maybe_z.valid)
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
            MaybeFloat maybe_x = string_to_float(x);
            texcoord.x = maybe_x.value;
            texcoord.y = 0.0f;
            texcoord.z = 0.0f;
            bool success_x = maybe_x.valid;
            bool success_y = true;
            bool success_z = true;
            if(y)
            {
                MaybeFloat maybe_y = string_to_float(y);
                texcoord.y = maybe_y.value;
                success_y = maybe_y.valid;
            }
            if(z)
            {
                MaybeFloat maybe_z = string_to_float(z);
                texcoord.z = maybe_z.value;
                success_z = maybe_z.valid;
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
                MaybeInt position = string_to_int(position_index);
                index.position = position.value;
                index.texcoord = index.position;
                index.normal = index.position;

                index.position = fix_index(index.position, array_count(positions));
                bool success_texcoord = true;
                bool success_normal = true;
                if(texcoord_index)
                {
                    MaybeInt texcoord = string_to_int(texcoord_index);
                    success_texcoord = texcoord.valid;
                    index.texcoord = texcoord.value;
                    index.texcoord = fix_index(index.texcoord, array_count(texcoords));
                }
                if(normal_index)
                {
                    MaybeInt normal = string_to_int(normal_index);
                    success_normal = normal.valid;
                    index.texcoord = normal.value;
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

                if(!position.valid || !success_texcoord || !success_normal)
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
                MaybeInt maybe_group = string_to_int(token);
                success_group = maybe_group.valid;
                group = maybe_group.value;
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

    STACK_DEALLOCATE(stack, whole_file.contents);

    error_occurred = error_occurred || array_count(positions) == 0;

    // Now that the file is finished processing, open the material library and
    // match up its contents to the stored material data.

    // TODO: material library

    // Fill the mesh with the completed data.
    if(!error_occurred)
    {
        JanMesh mesh;
        jan_create_mesh(&mesh);
        JanVertex** seen = STACK_ALLOCATE(stack, JanVertex*, array_count(positions));
        for(int i = 0; i < array_count(faces); i += 1)
        {
            Face obj_face = faces[i];
            JanVertex** vertices = STACK_ALLOCATE(stack, JanVertex*, obj_face.sides);
            for(int j = 0; j < obj_face.sides; j += 1)
            {
                MultiIndex index = multi_indices[obj_face.base_index + j];
                Float4 position = positions[index.position];
#if 0
                Float3 normal = normals[index.normal];
                Float3 texcoord = texcoords[index.texcoord];
#endif
                JanVertex* vertex;
                if(seen[index.position])
                {
                    vertex = seen[index.position];
                }
                else
                {
                    vertex = jan_add_vertex(&mesh, float4_extract_float3(position));
                    seen[index.position] = vertex;
                }
                vertices[j] = vertex;
            }
            jan_connect_disconnected_vertices_and_add_face(&mesh, vertices, obj_face.sides, stack);
            STACK_DEALLOCATE(stack, vertices);
        }
        jan_update_normals(&mesh);
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

static bool vertex_attached_to_face(JanVertex* vertex)
{
    return vertex->any_edge && vertex->any_edge->any_link;
}

#define LINE_SIZE 128

bool obj_save_file(const char* path, JanMesh* mesh, Heap* heap)
{
    File* file = open_file(NULL, FILE_OPEN_MODE_WRITE_TEMPORARY, heap);

    char line[LINE_SIZE];

    Map map;
    map_create(&map, mesh->vertices_count, heap);

    int index = 1;
    FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
    {
        if(!vertex_attached_to_face(vertex))
        {
            continue;
        }

        Float3 v = vertex->position;
        format_string(line, LINE_SIZE, "v %.6f %.6f %.6f\n", v.x, v.y, v.z);
        write_file(file, line, string_size(line));

        void* value = (void*) (uintptr_t) index;
        map_add(&map, vertex, value, heap);
        index += 1;
    }

    const char* usemtl = "usemtl None\n";
    write_file(file, usemtl, string_size(usemtl));

    const char* s = "s off\n";
    write_file(file, s, string_size(s));

    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        int i = 0;
        int line_left = LINE_SIZE;
        int copied = copy_string(&line[i], line_left, "f");
        i += copied;
        line_left -= copied;

        // @Incomplete: This outputs a filled face without any holes that might
        // be in it. .obj doesn't support holes, so the most reasonable way to
        // handle this would be to detect if the face has holes and, if it does,
        // split it into multiple faces.
        ASSERT(!face->first_border->next);

        JanLink* first = face->first_border->first;
        JanLink* link = first;
        do
        {
            MaybePointer result = map_get(&map, link->vertex);
            uintptr_t index = (uintptr_t) result.value;
            char text[22];
            text[0] = ' ';
            int_to_string(text + 1, 21, (int) index);
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
