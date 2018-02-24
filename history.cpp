#include "history.h"

#include "memory.h"
#include "int_utilities.h"
#include "logging.h"
#include "assert.h"
#include "loop_macros.h"

void history_create(History* history, Heap* heap)
{
    const int base_states_cap = 2;
    const int changes_cap = 200;

    history->base_states = HEAP_ALLOCATE(heap, Change, base_states_cap);
    history->changes = HEAP_ALLOCATE(heap, Change, changes_cap);
    history->base_states_cap = base_states_cap;
    history->base_states_count = 0;
    history->changes_cap = changes_cap;
    history->head = 0;
    history->tail = 0;
    history->index = 0;
}

void history_destroy(History* history, Heap* heap)
{
    if(history)
    {
        SAFE_HEAP_DEALLOCATE(heap, history->base_states);
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
    history->changes[history->head] = change;
    history->head = (history->head + 1) % history->changes_cap;

    if(history->head == history->tail)
    {
        history->tail = (history->tail + 1) % history->changes_cap;
    }
}

void history_add_base_state(History* history, Change change)
{
    history->base_states[history->base_states_count] = change;
    history->base_states_count += 1;
    ASSERT(history->base_states_count <= history->base_states_cap);
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

            FOR_N(i, history->base_states_cap)
            {
                Change* base = &history->base_states[i];
                if(base->type == change.type && base->move.object_id == object_id)
                {
                    return base;
                }
            }

            ASSERT(false); // base state wasn't set
            break;
        }
    }
    return nullptr;
}

