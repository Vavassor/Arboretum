#ifndef HISTORY_H_
#define HISTORY_H_

#include "editor.h"
#include "log.h"
#include "memory.h"
#include "object.h"
#include "vector_math.h"

typedef enum ChangeType
{
    CHANGE_TYPE_INVALID,
    CHANGE_TYPE_CREATE_OBJECT,
    CHANGE_TYPE_DELETE_OBJECT,
    CHANGE_TYPE_MOVE,
} ChangeType;

typedef struct Change
{
    union
    {
        struct
        {
            ObjectId object_id;
        } create_object;

        struct
        {
            ObjectId object_id;
        } delete_object;

        struct
        {
            Float3 position;
            ObjectId object_id;
        } move;
    };
    ChangeType type;
} Change;

typedef struct History
{
    Change* base_states;
    Change* changes;
    Change* changes_to_clean_up;
    int changes_cap;
    int changes_to_clean_up_cap;
    int changes_to_clean_up_count;
    int head;
    int tail;
    int index;
} History;

void history_create(History* history, Heap* heap);
void history_destroy(History* history, Heap* heap);
bool history_is_empty(History* history);
bool history_is_at_start(History* history);
bool history_is_at_end(History* history);
void history_add(History* history, Change change);
void history_add_base_state(History* history, Change change, Heap* heap);
void history_remove_base_state(History* history, ObjectId object_id);
Change* history_get(History* history, int index);
void history_set_index(History* history, int index);
void history_step(History* history, int step);
Change* history_find_past_change(History* history);
void history_log(History* history, Log* logger);

void add_object_to_history(History* history, struct Object* object, Heap* heap);
void undo(History* history, struct ObjectLady* lady, VideoContext* context, Heap* heap, Editor* editor, Platform* platform);
void redo(History* history, struct ObjectLady* lady, VideoContext* context, Heap* heap);

#endif // HISTORY_H_
