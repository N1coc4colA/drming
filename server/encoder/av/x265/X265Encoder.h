#ifndef DRMING_X265ENCODER_H
#define DRMING_X265ENCODER_H

#include "../encoder.h"

namespace X265
{
    class X265Encoder
    {
    public:
        /// Configures libx265 encoder parameters on the given codec context.
        static AVPixelFormat setup(AVCodecContext *ctx);
    };
} // namespace X265


#endif //DRMING_X265ENCODER_H
