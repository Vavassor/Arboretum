#ifndef DEBUG_READOUT_H_
#define DEBUG_READOUT_H_

#include <stdbool.h>

#define DEBUG_CHANNEL_CAP       4
#define DEBUG_CHANNEL_LABEL_CAP 128
#define DEBUG_CHANNEL_VALUE_CAP 128

typedef enum DebugChannelType
{
    DEBUG_CHANNEL_TYPE_INVALID,
    DEBUG_CHANNEL_TYPE_FLOAT,
} DebugChannelType;

typedef struct DebugChannel
{
    union
    {
        struct
        {
            float floats[DEBUG_CHANNEL_VALUE_CAP];
            float float_min;
            float float_max;
        };
    };
    char label[DEBUG_CHANNEL_LABEL_CAP];
    DebugChannelType type;
} DebugChannel;

typedef struct DebugReadout
{
    DebugChannel channels[DEBUG_CHANNEL_CAP];
    int index;
} DebugReadout;

void debug_readout_float(int channel_index, const char* label, float value);
void debug_readout_reset();
void debug_readout_update_ranges();

extern DebugReadout debug_readout;

#endif // DEBUG_READOUT_H_
