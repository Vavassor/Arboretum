#ifndef VIDEO_H_
#define VIDEO_H_

namespace video {

bool system_startup();
void system_shutdown(bool functions_loaded);
void system_update();
void resize_viewport(int width, int height);

} // namespace video

#endif // VIDEO_H_
