#include "debug_readout.h"

#include "assert.h"
#include "float_utilities.h"
#include "math_basics.h"
#include "memory.h"
#include "string_utilities.h"

DebugReadout debug_readout;

void debug_readout_float(int channel_index, const char* label, float value)
{
    ASSERT(channel_index >= 0 && channel_index < DEBUG_CHANNEL_CAP);
    ASSERT(debug_readout.index >= 0 && debug_readout.index < DEBUG_CHANNEL_VALUE_CAP);

    DebugChannel* channel = &debug_readout.channels[channel_index];

    if(channel->type != DEBUG_CHANNEL_TYPE_FLOAT)
    {
        channel->type = DEBUG_CHANNEL_TYPE_FLOAT;
        set_memory(channel->floats, 0, sizeof(float) * DEBUG_CHANNEL_VALUE_CAP);
        set_memory(channel->label, 0, DEBUG_CHANNEL_LABEL_CAP);
    }

    channel->floats[debug_readout.index] = value;
    format_string(channel->label, DEBUG_CHANNEL_LABEL_CAP, "%s: %f", label, value);
}

void debug_readout_reset()
{
    debug_readout.index = (debug_readout.index + 1) % DEBUG_CHANNEL_VALUE_CAP;
}

void debug_readout_update_ranges()
{
    for(int channel_index = 0;
            channel_index < DEBUG_CHANNEL_CAP;
            channel_index += 1)
    {
        DebugChannel* channel = &debug_readout.channels[channel_index];
        switch(channel->type)
        {
            case DEBUG_CHANNEL_TYPE_FLOAT:
            {
                float min = +infinity;
                float max = -infinity;
                for(int value_index = 0;
                        value_index < DEBUG_CHANNEL_VALUE_CAP;
                        value_index += 1)
                {
                    float value = channel->floats[value_index];
                    min = fminf(min, value);
                    max = fmaxf(max, value);
                }
                channel->float_min = min;
                channel->float_max = max;
                break;
            }
            case DEBUG_CHANNEL_TYPE_INVALID:
            {
                break;
            }
        }
    }
}
