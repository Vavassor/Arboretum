#ifndef PLATFORM_H_
#define PLATFORM_H_

enum class CursorType
{
    Arrow,
    Hand_Pointing,
};

struct Platform
{
    int placeholder;
};

void change_cursor(Platform* platform, CursorType type);
void begin_composed_text(Platform* platform);
void end_composed_text(Platform* platform);

#endif // PLATFORM_H_
