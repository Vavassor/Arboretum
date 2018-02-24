#ifndef HISTORY_H_
#define HISTORY_H_

#include "vector_math.h"
#include "object.h"

enum class ChangeType
{
    Create_Object,
    Delete_Object,
    Move,
};

struct Change
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
            Vector3 position;
            ObjectId object_id;
        } move;
    };
    ChangeType type;
};

struct History
{
    Change* base_states;
    Change* changes;
    Change* changes_to_clean_up;
    int base_states_cap;
    int base_states_count;
    int changes_cap;
    int changes_to_clean_up_cap;
    int changes_to_clean_up_count;
    int head;
    int tail;
    int index;
};

struct Heap;

void history_create(History* history, Heap* heap);
void history_destroy(History* history, Heap* heap);
bool history_is_empty(History* history);
bool history_is_at_start(History* history);
bool history_is_at_end(History* history);
void history_add(History* history, Change change);
void history_add_base_state(History* history, Change change, Heap* heap);
Change* history_get(History* history, int index);
void history_set_index(History* history, int index);
void history_step(History* history, int step);
Change* history_find_past_change(History* history);
void history_log(History* history);

#endif // HISTORY_H_
