#ifndef LOC_H_
#define LOC_H_

struct Platform;

// Localized Text File (.loc)
namespace loc {

bool load_file(Platform* platform);

} // namespace loc

#endif // LOC_H_
