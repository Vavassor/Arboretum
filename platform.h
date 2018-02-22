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

#endif // PLATFORM_H_
