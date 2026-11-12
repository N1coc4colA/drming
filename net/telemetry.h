#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QtGlobal>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>

#include "../helpers.h"

/// Returns the current steady clock timestamp in nanoseconds.
inline quint64 steadyNowNs()
{
    // I do not see how the system's time may be negative...
    return static_cast<quint64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}

/// Aggregates network streaming, decode latency, and queue depth telemetry metrics.
class Telemetry
{
public:
    /// Snapshot of aggregated telemetry metrics over a measurement window.
    struct Snapshot
    {
        /// Decoded frame rate multiplied by 10 (e.g. 600 = 60.0 fps).
        quint16 fpsX10 = 0;
        /// Average end-to-end frame latency in milliseconds.
        quint16 latencyMsAvg = 0;
        /// 95th-percentile frame latency in milliseconds.
        quint16 latencyMsP95 = 0;
        /// Number of dropped frames during the snapshot window.
        quint16 droppedFrames = 0;
        /// Network receive data rate in kilobits per second.
        quint16 recvRateKbps = 0;
        /// Current depth of the decoder input queue.
        quint8 decodeQueueDepth = 0;
        /// Current depth of the presentation render queue.
        quint8 renderQueueDepth = 0;
        /// Total number of frames received during the snapshot window.
        quint32 receivedFrames = 0;
    };

    /// Records an incoming network frame and its byte payload size.
    void recordReceived(const quint64 bytes)
    {
        m_framesReceived.fetch_add(1, std::memory_order_relaxed);
        m_bytesReceived.fetch_add(bytes, std::memory_order_relaxed);
    }

    /// Records the decode latency of a single frame in nanoseconds.
    void recordDecoded(const quint64 latencyNs)
    {
        const auto ms = latencyNs / 1'000'000;
        if (ms >= 0 && ms < to_signed<qint64>(kHistogramBuckets)) {
            m_latencyHistogram[checked_cast<size_t>(ms)].fetch_add(1, std::memory_order_relaxed);
        } else if (ms >= to_signed<qint64>(kHistogramBuckets)) {
            // Clamp to last bucket rather than lose the sample.
            m_latencyHistogram[kHistogramBuckets - 1].fetch_add(1, std::memory_order_relaxed);
        }

        m_framesDecoded.fetch_add(1, std::memory_order_relaxed);
    }

    /// Records a dropped frame event.
    void recordDropped()
    {
        m_framesDropped.fetch_add(1, std::memory_order_relaxed);
    }

    /// Updates the instantaneous decoder queue depth.
    void setDecodeQueueDepth(const quint8 d)
    {
        m_decodeQueueDepth.store(d, std::memory_order_relaxed);
    }
    /// Updates the instantaneous render presentation queue depth.
    void setRenderQueueDepth(const quint8 d)
    {
        m_renderQueueDepth.store(d, std::memory_order_relaxed);
    }

    /// Computes and returns an aggregated telemetry snapshot over the given time window in nanoseconds.
    Snapshot takeSnapshot(const quint64 windowNs)
    {
        Snapshot s{};
        if (windowNs <= 0) {
            return s;
        }

        const auto framesDecoded = m_framesDecoded.exchange(0, std::memory_order_relaxed);
        const auto framesDropped = m_framesDropped.exchange(0, std::memory_order_relaxed);
        const auto bytesReceived = m_bytesReceived.exchange(0, std::memory_order_relaxed);
        const auto framesReceived = m_framesReceived.exchange(0, std::memory_order_relaxed);

        s.receivedFrames = framesReceived;

        const double fps = framesDecoded * 1e9 / static_cast<double>(windowNs);
        s.fpsX10 = static_cast<quint16>(std::min(1000.0, fps * 10.0));
        s.droppedFrames = static_cast<quint16>(std::min<quint32>(65535, framesDropped));

        const double kbps = static_cast<double>(bytesReceived) * 8.0 * 1e9 / static_cast<double>(windowNs) / 1000.0;
        s.recvRateKbps = static_cast<quint16>(std::min(65535.0, kbps));

        quint32 counts[kHistogramBuckets]{};
        quint32 totalSamples = 0;
        quint64 sumMs = 0;
        for (size_t i = 0; i < kHistogramBuckets; ++i) {
            counts[i] = m_latencyHistogram[i].exchange(0, std::memory_order_relaxed);
            totalSamples += counts[i];
            sumMs += i * counts[i];
        }

        if (totalSamples > 0) {
            s.latencyMsAvg = static_cast<quint16>(std::min<quint64>(65535, sumMs / totalSamples));
            const quint32 target = (totalSamples * 95u + 99u) / 100u; // ceil
            quint32 acc = 0;
            for (quint16 i = 0; i < kHistogramBuckets; ++i) {
                acc += counts[i];
                if (acc >= target) {
                    s.latencyMsP95 = i;
                    break;
                }
            }
        }

        s.decodeQueueDepth = m_decodeQueueDepth.load(std::memory_order_relaxed);
        s.renderQueueDepth = m_renderQueueDepth.load(std::memory_order_relaxed);

        return s;
    }

private:
    static constexpr quint16 kHistogramBuckets = 256; // 0..255 ms, 1 ms resolution

    std::atomic<quint32> m_framesReceived{ 0 };
    std::atomic<quint32> m_framesDecoded{ 0 };
    std::atomic<quint32> m_framesDropped{ 0 };
    std::atomic<quint64> m_bytesReceived{ 0 };
    std::array<std::atomic<quint32>, kHistogramBuckets> m_latencyHistogram{};
    std::atomic<quint8> m_decodeQueueDepth{ 0 };
    std::atomic<quint8> m_renderQueueDepth{ 0 };
};

#endif // TELEMETRY_H
