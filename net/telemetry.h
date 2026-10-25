#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QtGlobal>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>

inline qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class Telemetry
{
public:
    struct Snapshot
    {
        quint16 fpsX10 = 0;
        quint16 latencyMsAvg = 0;
        quint16 latencyMsP95 = 0;
        quint16 droppedFrames = 0;
        quint16 recvRateKbps = 0;
        quint8 decodeQueueDepth = 0;
        quint8 renderQueueDepth = 0;
    };

    void recordReceived(const qint64 bytes)
    {
        m_framesReceived.fetch_add(1, std::memory_order_relaxed);
        m_bytesReceived.fetch_add(static_cast<quint64>(std::max<qint64>(0, bytes)), std::memory_order_relaxed);
    }

    void recordDecoded(const qint64 latencyNs)
    {
        const auto ms = latencyNs / 1'000'000;
        if (ms >= 0 && ms < static_cast<qint64>(kHistogramBuckets)) {
            m_latencyHistogram[ms].fetch_add(1, std::memory_order_relaxed);
        } else if (ms >= static_cast<qint64>(kHistogramBuckets)) {
            // Clamp to last bucket rather than lose the sample.
            m_latencyHistogram[kHistogramBuckets - 1].fetch_add(1, std::memory_order_relaxed);
        }

        m_framesDecoded.fetch_add(1, std::memory_order_relaxed);
    }

    void recordDropped()
    {
        m_framesDropped.fetch_add(1, std::memory_order_relaxed);
    }

    void setDecodeQueueDepth(const quint8 d)
    {
        m_decodeQueueDepth.store(d, std::memory_order_relaxed);
    }
    void setRenderQueueDepth(const quint8 d)
    {
        m_renderQueueDepth.store(d, std::memory_order_relaxed);
    }

    Snapshot takeSnapshot(const qint64 windowNs)
    {
        Snapshot s{};
        if (windowNs <= 0) {
            return s;
        }

        const auto framesDecoded = m_framesDecoded.exchange(0, std::memory_order_relaxed);
        const auto framesDropped = m_framesDropped.exchange(0, std::memory_order_relaxed);
        const auto bytesReceived = m_bytesReceived.exchange(0, std::memory_order_relaxed);
        (void)m_framesReceived.exchange(0, std::memory_order_relaxed);

        const double fps = framesDecoded * 1e9 / static_cast<double>(windowNs);
        s.fpsX10 = static_cast<quint16>(std::min(1000.0, fps * 10.0));
        s.droppedFrames = static_cast<quint16>(std::min<quint32>(65535, framesDropped));

        const double kbps = bytesReceived * 8.0 * 1e9 / static_cast<double>(windowNs) / 1000.0;
        s.recvRateKbps = static_cast<quint16>(std::min(65535.0, kbps));

        quint32 counts[kHistogramBuckets]{};
        quint32 totalSamples = 0;
        quint64 sumMs = 0;
        for (int i = 0; i < kHistogramBuckets; ++i) {
            counts[i] = m_latencyHistogram[i].exchange(0, std::memory_order_relaxed);
            totalSamples += counts[i];
            sumMs += quint64(i) * counts[i];
        }

        if (totalSamples > 0) {
            s.latencyMsAvg = static_cast<quint16>(std::min<quint64>(65535, sumMs / totalSamples));
            const quint32 target = (totalSamples * 95u + 99u) / 100u; // ceil
            quint32 acc = 0;
            for (int i = 0; i < kHistogramBuckets; ++i) {
                acc += counts[i];
                if (acc >= target) {
                    s.latencyMsP95 = static_cast<quint16>(i);
                    break;
                }
            }
        }

        s.decodeQueueDepth = m_decodeQueueDepth.load(std::memory_order_relaxed);
        s.renderQueueDepth = m_renderQueueDepth.load(std::memory_order_relaxed);

        return s;
    }

private:
    static constexpr int kHistogramBuckets = 256; // 0..255 ms, 1 ms resolution

    std::atomic<quint32> m_framesReceived{ 0 };
    std::atomic<quint32> m_framesDecoded{ 0 };
    std::atomic<quint32> m_framesDropped{ 0 };
    std::atomic<quint64> m_bytesReceived{ 0 };
    std::array<std::atomic<quint32>, kHistogramBuckets> m_latencyHistogram{};
    std::atomic<quint8> m_decodeQueueDepth{ 0 };
    std::atomic<quint8> m_renderQueueDepth{ 0 };
};

#endif // TELEMETRY_H
