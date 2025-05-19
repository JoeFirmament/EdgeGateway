#include "camera/frame.h"

namespace cam_server {
namespace camera {

Frame::Frame(int width, int height, PixelFormat format, std::vector<uint8_t> data)
    : width_(width)
    , height_(height)
    , format_(format)
    , data_(std::move(data)) {
}

} // namespace camera
} // namespace cam_server 