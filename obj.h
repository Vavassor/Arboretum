#ifndef OBJ_H_
#define OBJ_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "jan.h"
#include "memory.h"

// Wavefront Object File (.obj)

bool obj_load_file(const char* path, JanMesh* mesh, Heap* heap, Stack* stack);
bool obj_save_file(const char* path, JanMesh* mesh, Heap* heap);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // OBJ_H_
