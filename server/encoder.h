#ifndef DRMING_ENCODER_H
#define DRMING_ENCODER_H

#include <cstdint>
#include <functional>

#include <QImage>

/**
 * Converts captured DRM images into the negotiated compressed stream format.
 *
 * The encoder is initialized lazily from the first frame because the display
 * dimensions and pixel format are properties of the active connector.
 */
class Encoder
{
public:
    using PacketCallback = std::function<void(const uint8_t *data, int size, int64_t pts)>;
    using EncoderCreator = Encoder *(*)(PacketCallback callback, int fps);

    class Entry
    {
    public:
        explicit Entry(const char *const name, const bool hasHWA, const EncoderCreator creator)
            : m_name(name)
            , m_creator(creator)
            , m_hasHWA(hasHWA)
        {
            assert(name != nullptr);
            assert(creator != nullptr);
        }

        [[nodiscard]] const char *getName() const
        {
            return m_name;
        }

        [[nodiscard]] EncoderCreator getCreationFunction() const
        {
            return m_creator;
        }

        [[nodiscard]] bool hasHWA() const
        {
            return m_hasHWA;
        }

        void init() const
        {}

    private:
        const char *const m_name;
        const EncoderCreator m_creator;
        const bool m_hasHWA;
    };

    virtual ~Encoder() = default;
    /// Encodes one frame and invokes the callback for each produced packet.
    virtual void push_image(const uint8_t *data, int width, int height, QImage::Format format, qsizetype stride) = 0;

    /// Forces the next encoded frame to be an IDR. Safe to call from any thread.
    virtual void forceKeyframe() = 0;

    static Encoder *createEncoder(PacketCallback callback, int fps);

protected:
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int64_t m_pts = 0;

    PacketCallback m_callback{};

    explicit Encoder(PacketCallback callback, const int fps)
        : m_fps(fps)
        , m_callback(std::move(callback))
    {}
};

#endif //DRMING_ENCODER_H
