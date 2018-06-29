#include "history.h"

#include "array2.h"
#include "assert.h"
#include "editor.h"
#include "int_utilities.h"
#include "logging.h"
#include "memory.h"
#include "object_lady.h"

void history_create(History* history, Heap* heap)
{
    const int changes_cap = 200;
    const int cleanup_cap = 200;

    history->base_states = nullptr;
    history->changes = HEAP_ALLOCATE(heap, Change, changes_cap);
    history->changes_to_clean_up = HEAP_ALLOCATE(heap, Change, cleanup_cap);
    history->changes_cap = changes_cap;
    history->changes_to_clean_up_cap = cleanup_cap;
    history->changes_to_clean_up_count = 0;
    history->head = 0;
    history->tail = 0;
    history->index = 0;
}

void history_destroy(History* history, Heap* heap)
{
    if(history)
    {
        ARRAY_DESTROY(history->base_states, heap);
        SAFE_HEAP_DEALLOCATE(heap, history->changes);
    }
}

bool history_is_empty(History* history)
{
    return history->head == history->tail;
}

bool history_is_at_start(History* history)
{
    return history->index == history->tail;
}

bool history_is_at_end(History* history)
{
    return history->index == history->head;
}

void history_add(History* history, Change change)
{
    history->head = history->index;

    Change save = history->changes[history->head];
    history->changes[history->head] = change;
    history->head = (history->head + 1) % history->changes_cap;

    if(history->head == history->tail)
    {
        if(save.type != ChangeType::Invalid)
        {
            history->changes_to_clean_up[history->changes_to_clean_up_count] = save;
            history->changes_to_clean_up_count += 1;
        }
        history->tail = (history->tail + 1) % history->changes_cap;
    }

    history_step(history, +1);
}

void history_add_base_state(History* history, Change change, Heap* heap)
{
    ARRAY_ADD(history->base_states, change, heap);
}

void history_remove_base_state(History* history, ObjectId object_id)
{
    FOR_ALL(Change, history->base_states)
    {
        ASSERT(it->type == ChangeType::Move);
        if(it->move.object_id == object_id)
        {
            ARRAY_REMOVE(history->base_states, it);
            break;
        }
    }
}

static bool in_cyclic_interval(int x, int first, int second)
{
    if(second > first)
    {
        return x >= first && x <= second;
    }
    else
    {
        return x >= first || x <= second;
    }
}

Change* history_get(History* history, int index)
{
    if(in_cyclic_interval(index, history->tail, history->head))
    {
        return &history->changes[index];
    }
    else
    {
        return nullptr;
    }
}

void history_set_index(History* history, int index)
{
    if(in_cyclic_interval(index, history->tail, history->head))
    {
        history->index = index;
    }
}

void history_step(History* history, int step)
{
    int index = mod(history->index + step, history->changes_cap);
    history_set_index(history, index);
}

Change* history_find_past_change(History* history)
{
    Change change = history->changes[history->index];

    switch(change.type)
    {
        case ChangeType::Invalid:
        {
            ASSERT(false);
            break;
        }
        case ChangeType::Create_Object:
        case ChangeType::Delete_Object:
        {
            return &history->changes[history->index];
        }
        case ChangeType::Move:
        {
            ObjectId object_id = change.move.object_id;

            if(history->index != history->tail)
            {
                int i = mod(history->index - 1, history->changes_cap);
                for(;;)
                {
                    Change* candidate = &history->changes[i];
                    if(candidate->type == change.type && candidate->move.object_id == object_id)
                    {
                        return candidate;
                    }
                    if(i == history->tail)
                    {
                        break;
                    }
                    i = mod(i - 1, history->changes_cap);
                }
            }

            FOR_ALL(Change, history->base_states)
            {
                if(it->type == change.type && it->move.object_id == object_id)
                {
                    return it;
                }
            }

            ASSERT(false); // base state wasn't set
            break;
        }
    }
    return nullptr;
}

void history_log(History* history)
{
    for(int i = history->tail; i != history->head; i = (i + 1) % history->changes_cap)
    {
        Change change = history->changes[i];
        const char* pointer = "";
        if(i == history->index)
        {
            pointer = " <----";
        }
        switch(change.type)
        {
            case ChangeType::Invalid:
            {
                ASSERT(false);
                break;
            }
            case ChangeType::Create_Object:
            {
                LOG_DEBUG("Create_Object object = %u%s", change.create_object.object_id, pointer);
                break;
            }
            case ChangeType::Delete_Object:
            {
                LOG_DEBUG("Delete_Object object = %u%s", change.delete_object.object_id, pointer);
                break;
            }
            case ChangeType::Move:
            {
                LOG_DEBUG("Move object = %u position = <%f, %f, %f>%s", change.move.object_id, change.move.position.x, change.move.position.y, change.move.position.z, pointer);
                break;
            }
        }
    }
    LOG_DEBUG("");
}

void add_object_to_history(History* history, Object* object, Heap* heap)
{
    Change state;
    state.type = ChangeType::Move;
    state.move.object_id = object->id;
    state.move.position = object->position;
    history_add_base_state(history, state, heap);
}

void undo(History* history, ObjectLady* lady, Heap* heap, Platform* platform)
{
    if(history_is_at_start(history) || history_is_empty(history))
    {
        return;
    }

    history_step(history, -1);

    Change* change = history_find_past_change(history);
    switch(change->type)
    {
        case ChangeType::Invalid:
        {
            ASSERT(false);
            break;
        }
        case ChangeType::Create_Object:
        {
            ObjectId id = change->create_object.object_id;
            clear_object_from_hover_and_selection(id, platform);
            store_object(lady, id, heap);
            break;
        }
        case ChangeType::Delete_Object:
        {
            take_out_of_storage(lady, change->delete_object.object_id, heap);
            break;
        }
        case ChangeType::Move:
        {
            ObjectId id = change->move.object_id;
            Object* object = get_object_by_id(lady, id);
            object_set_position(object, change->move.position);
            break;
        }
    }
}

void redo(History* history, ObjectLady* lady, Heap* heap)
{
    if(history_is_at_end(history))
    {
        return;
    }

    Change* change = history_get(history, history->index);
    switch(change->type)
    {
        case ChangeType::Invalid:
        {
            ASSERT(false);
            break;
        }
        case ChangeType::Create_Object:
        {
            take_out_of_storage(lady, change->create_object.object_id, heap);
            break;
        }
        case ChangeType::Delete_Object:
        {
            store_object(lady, change->delete_object.object_id, heap);
            break;
        }
        case ChangeType::Move:
        {
            ObjectId id = change->move.object_id;
            Object* object = get_object_by_id(lady, id);
            object_set_position(object, change->move.position);
            break;
        }
    }

    history_step(history, +1);
}
