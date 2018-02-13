#ifndef VIDEO_H_
#define VIDEO_H_

struct Platform;

namespace video {

bool system_startup();
void system_shutdown(bool functions_loaded);
void system_update(Platform* platform);
void resize_viewport(int width, int height, double dots_per_millimeter);

} // namespace video

#endif // VIDEO_H_
