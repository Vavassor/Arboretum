#ifndef OBJ_H_
#define OBJ_H_

namespace jan {

struct Mesh;

} // namespace jan

struct Stack;
struct Heap;

// Wavefront Object File (.obj)
namespace obj {

bool load_file(const char* path, jan::Mesh* mesh, Heap* heap, Stack* stack);
bool save_file(const char* path, jan::Mesh* mesh, Heap* heap);

} // namespace obj

#endif // OBJ_H_
