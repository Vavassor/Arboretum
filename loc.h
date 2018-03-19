#ifndef LOC_H_
#define LOC_H_

struct Platform;

// Localized Text File (.loc)
namespace loc {

bool load_file(Platform* platform, const char* path);

} // namespace loc

#endif // LOC_H_
