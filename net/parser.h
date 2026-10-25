#ifndef PARSER_H
#define PARSER_H

#include <QByteArray>
#include <QDataStream>
#include <QDebug>

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#ifndef Q_MOC_RUN
#include <ctrack.hpp>
#endif

namespace Packets {

using Ordering = quint32;
using Timestamp = quint16;
using Keystamp = qint16;

enum class Type : quint16 {
    None = 0,

    HeartBeat,
    Reinit,

    RequestKey,
    KeyUpdate,

    RequestAudioSource,
    AudioSource,
    RequestVideoSource,
    VideoSource,
    RequestClientResolution,
    ClientResolution,

    ServerBrightness,
    ServerImage,
    ServerStream,

    SpeedInfo,
    ClientTelemetry,
    RequestKeyframe,

    MINIMUM = HeartBeat,
    MAXIMUM = RequestKeyframe,

    LOWER = HeartBeat,
    UPPER = MAXIMUM,
};

namespace details {

template<Type Wanted, typename... Ts>
struct find_packet;

template<Type Wanted, typename Head, typename... Tail>
struct find_packet<Wanted, Head, Tail...>
{
    using type = std::conditional_t<Head::type == Wanted, Head, typename find_packet<Wanted, Tail...>::type>;
};

template<Type Wanted>
struct find_packet<Wanted>
{
    static_assert(Wanted != Wanted, "No packet type matches this enum value");
};

template<Type Wanted, typename... Ts>
using find_packet_t = find_packet<Wanted, Ts...>::type;

template<typename>
struct variant_types;

template<typename... Ts>
struct variant_types<std::variant<Ts...>>
{
    template<Type Wanted>
    using find = find_packet<Wanted, Ts...>::type;
    using types = std::tuple<Ts...>;
};

template<class T, template<class...> class U>
inline constexpr bool is_instance_of_v = std::false_type{};

template<template<class...> class U, class... Vs>
inline constexpr bool is_instance_of_v<U<Vs...>, U> = std::true_type{};

template<typename Receiver, typename Packet, typename = void>
struct has_processPacket : std::false_type
{
};

template<typename Receiver, typename Packet>
struct has_processPacket<
    Receiver,
    Packet,
    std::void_t<decltype(std::declval<Receiver>().processPacket(std::declval<const Packet &>()))>> : std::true_type
{
};

template<typename Receiver, typename = void>
struct has_packetErrors : std::false_type
{
};

template<typename Receiver>
struct has_packetErrors<Receiver, std::void_t<decltype(std::declval<Receiver>().packetErrors())>> : std::true_type
{
};

template<class T, class Tuple>
struct Index;

template<class T>
struct Index<T, std::tuple<>>
{
    static_assert(sizeof(T) == 0, "Type not found in tuple");
};

template<class T, class... Types>
struct Index<T, std::tuple<T, Types...>>
{
    static constexpr std::size_t value = 0;
};

template<class T, class U, class... Types>
struct Index<T, std::tuple<U, Types...>>
{
    static constexpr std::size_t value = 1 + Index<T, std::tuple<Types...>>::value;
};

template<typename T>
struct member_pointer_traits;

template<typename M, typename C>
struct member_pointer_traits<M C::*>
{
    using member_type = M;
    using class_type = C;
};

template<typename M, typename C>
struct member_pointer_traits<M C::*const>
{
    using member_type = M;
    using class_type = C;
};

template<typename M, typename C>
struct member_pointer_traits<const M C::*>
{
    using member_type = const M;
    using class_type = C;
};

template<typename M, typename C>
struct member_pointer_traits<const M C::*const>
{
    using member_type = const M;
    using class_type = C;
};

template<typename PacketType>
using fields_type = std::remove_reference_t<decltype(PacketType::fields)>;

} // namespace details

template<typename T, bool IsVariable = false, bool HasLimit = false>
struct Field
{
    using data_type = T;

    static constexpr bool __is_field = true;
    static constexpr bool __is_variable = IsVariable;
    static constexpr bool __has_limits = HasLimit;

    T data{};

    inline T &operator->()
    {
        return data;
    }
    inline const T &operator->() const
    {
        return data;
    }
};

// SizedField implies IsVariable = true. Whether it has limits is decided by
// the concrete variable field templates below.
template<typename T, typename SizeType>
struct SizedField : public Field<T, true>
{
    using size_type = SizeType;
    SizeType size{};
};

// Fixed-size, compile-time-bounded field: not variable, has limits.
template<typename T, typename LimitType, LimitType Min, LimitType Max>
struct FixedField : Field<T, false, true>
{
    static constexpr LimitType min = Min;
    static constexpr LimitType max = Max;
};

// Fixed-size, runtime-bounded field: not variable, has limits.
template<
    typename T,
    typename LimitType,
    LimitType DefaultMin = 0,
    LimitType DefaultMax = std::numeric_limits<LimitType>::max()>
struct BoundField : public Field<T, false, true>
{
    LimitType min = DefaultMin;
    LimitType max = DefaultMax;

    inline constexpr void setBounds(const LimitType mn, const LimitType mx)
    {
        min = std::max(mn, DefaultMin);
        max = std::min(mx, DefaultMax);
    }
};

// Variable-size, compile-time-bounded field: variable, has limits.
template<typename T, typename LimitType, LimitType Min, LimitType Max>
struct VariableFixedField : SizedField<T, LimitType>
{
    static constexpr bool __has_limits = true;

    static constexpr LimitType min = Min;
    static constexpr LimitType max = Max;
};

// Variable-size, runtime-bounded field: variable, has limits.
template<
    typename T,
    typename LimitType,
    LimitType DefaultMin = 0,
    LimitType DefaultMax = std::numeric_limits<LimitType>::max()>
struct VariableBoundField : public SizedField<T, LimitType>
{
    static constexpr bool __has_limits = true;

    LimitType min = DefaultMin;
    LimitType max = DefaultMax;

    inline constexpr void setBounds(const LimitType mn, const LimitType mx)
    {
        min = std::max(mn, DefaultMin);
        max = std::min(mx, DefaultMax);
    }
};

template<typename EnumType>
    requires requires() {
        std::is_enum_v<EnumType>;
        EnumType::UPPER;
        EnumType::LOWER;
        std::is_same_v<typename EnumType::UPPER, EnumType>;
        std::is_same_v<typename EnumType::LOWER, EnumType>;
    }
struct EnumField : public FixedField<EnumType, EnumType, EnumType::LOWER, EnumType::UPPER>
{
};

template<typename EnumType>
    requires requires() {
        std::is_enum_v<EnumType>;
        EnumType::UPPER;
        EnumType::LOWER;
        std::is_same_v<typename EnumType::UPPER, EnumType>;
        std::is_same_v<typename EnumType::LOWER, EnumType>;
    }
struct BoundEnumField : public BoundField<EnumType, EnumType, EnumType::LOWER, EnumType::UPPER>
{
};

template<
    typename NumericType,
    NumericType Min = std::numeric_limits<NumericType>::min(),
    NumericType Max = std::numeric_limits<NumericType>::max()>
struct NumericField : FixedField<NumericType, NumericType, Min, Max>
{
};

template<
    typename NumericType,
    NumericType DefaultMin = std::numeric_limits<NumericType>::min(),
    NumericType DefaultMax = std::numeric_limits<NumericType>::max()>
struct BoundNumericField : BoundField<NumericType, NumericType, DefaultMin, DefaultMax>
{
};

struct HeartBeat
{
    static constexpr auto type = Type::HeartBeat;
    static constexpr auto fields = std::tuple{};
};

struct Reinit
{
    static constexpr auto type = Type::Reinit;
    static constexpr auto fields = std::tuple{};
};

struct RequestKey
{
    static constexpr auto type = Type::RequestKey;
    static constexpr auto fields = std::tuple{};
};

struct KeyUpdate
{
    static constexpr auto type = Type::KeyUpdate;

    Field<Keystamp> keyStamp;
    VariableFixedField<QByteArray, quint64, 1, 2048> key;

    static constexpr auto fields = std::tuple{ &KeyUpdate::keyStamp, &KeyUpdate::key };
};

struct RequestAudioSource
{
    static constexpr auto type = Type::RequestAudioSource;
    static constexpr auto fields = std::tuple{};
};

struct AudioSource
{
    static constexpr auto type = Type::AudioSource;

    VariableFixedField<QByteArray, qsizetype, 4, 16> ip;
    Field<quint16> port;
    Field<bool> isV4;

    static constexpr auto fields = std::tuple{ &AudioSource::ip, &AudioSource::port, &AudioSource::isV4 };
};

struct RequestVideoSource
{
    static constexpr auto type = Type::RequestVideoSource;
    static constexpr auto fields = std::tuple{};
};

struct VideoSource
{
    static constexpr auto type = Type::VideoSource;

    VariableFixedField<QByteArray, qsizetype, 4, 16> ip;
    Field<quint16> port;
    Field<bool> isV4;

    static constexpr auto fields = std::tuple{ &VideoSource::ip, &VideoSource::port, &VideoSource::isV4 };
};

struct ServerImage
{
    static constexpr auto type = Type::ServerImage;

    VariableFixedField<QByteArray, quint64, 3, 4> format;
    VariableBoundField<QByteArray, quint64, 1, 8 * 1024 * 1024> data;

    static constexpr auto fields = std::tuple{ &ServerImage::format, &ServerImage::data };
};

struct RequestClientResolution
{
    static constexpr auto type = Type::RequestClientResolution;
    static constexpr auto fields = std::tuple{};
};

struct ClientResolution
{
    static constexpr auto type = Type::ClientResolution;

    NumericField<quint32, 0, 1920> width;
    NumericField<quint32, 0, 1080> height;

    static constexpr auto fields = std::tuple{ &ClientResolution::width, &ClientResolution::height };
};

struct ServerBrightness
{
    static constexpr auto type = Type::ServerBrightness;

    NumericField<std::int16_t, 0, 10> brightness;

    static constexpr auto fields = std::tuple{ &ServerBrightness::brightness };
};

struct ServerStream
{
    static constexpr auto type = Type::ServerStream;

    VariableBoundField<QByteArray, quint64, 1> data;

    static constexpr auto fields = std::tuple{ &ServerStream::data };
};

struct SpeedInfo
{
    static constexpr auto type = Type::SpeedInfo;

    NumericField<std::int16_t, 0, 200> speed;

    static constexpr auto fields = std::tuple{ &SpeedInfo::speed };
};

struct ClientTelemetry
{
    static constexpr auto type = Type::ClientTelemetry;

    /// actual decoded fps × 10
    NumericField<quint16, 0, 1000> fpsX10;
    /// avg end-to-end latency
    NumericField<quint16, 0, 65535> latencyMsAvg;
    /// 95th-percentile latency
    NumericField<quint16, 0, 65535> latencyMsP95;
    /// frames dropped by queues
    NumericField<quint16, 0, 65535> droppedFrames;
    /// video receive rate, Kbit/s
    NumericField<quint16, 0, 65535> recvRateKbps;
    /// current decoder queue
    NumericField<quint8, 0, 255> decodeQueueDepth;
    /// current render queue
    NumericField<quint8, 0, 255> renderQueueDepth;

    static constexpr auto fields = std::tuple{
        &ClientTelemetry::fpsX10,
        &ClientTelemetry::latencyMsAvg,
        &ClientTelemetry::latencyMsP95,
        &ClientTelemetry::droppedFrames,
        &ClientTelemetry::recvRateKbps,
        &ClientTelemetry::decodeQueueDepth,
        &ClientTelemetry::renderQueueDepth,
    };
};

struct RequestKeyframe
{
    static constexpr auto type = Type::RequestKeyframe;
    static constexpr auto fields = std::tuple{};
};

using PacketVariant = std::variant<
    HeartBeat,
    Reinit,
    RequestKey,
    KeyUpdate,
    RequestAudioSource,
    AudioSource,
    RequestVideoSource,
    VideoSource,
    RequestClientResolution,
    ClientResolution,
    ServerBrightness,
    ServerImage,
    ServerStream,
    SpeedInfo,
    ClientTelemetry,
    RequestKeyframe>;

class Writer
{
    template<typename FieldType>
        requires FieldType::__is_field and FieldType::__is_variable
    static void updateSizeIfNeeded(FieldType &field)
    {
        field.size = static_cast<FieldType::size_type>(field.data.size());
    }

    template<typename Arg>
    static void updateSizeIfNeeded(Arg &&)
    {}

    template<typename FieldType>
        requires FieldType::__is_field and FieldType::__is_variable
    static void writeField(QDataStream &stream, const FieldType &field)
    {
        stream << field.size;
    }

    template<typename FieldType>
        requires FieldType::__is_field and (not FieldType::__is_variable)
    static void writeField(QDataStream &stream, const FieldType &field)
    {
        stream << field.data;
    }

    template<typename FieldType>
    static void writeField(QDataStream &stream, const FieldType &field)
    {
        stream << field;
    }

    template<typename FieldType>
    static void writeVariableField(QDataStream &, const FieldType &)
    {}

    template<typename FieldType>
        requires FieldType::__is_field and FieldType::__is_variable
    static void writeVariableField(QDataStream &stream, const FieldType &field)
    {
        stream.writeRawData(field.data.constData(), static_cast<qint64>(field.size));
    }

    template<typename FieldType>
        requires FieldType::__is_field and FieldType::__is_variable
    static qsizetype estimateFieldSize(const FieldType &f)
    {
        return sizeof(typename FieldType::size_type) + f.data.size();
    }

    template<typename FieldType>
        requires FieldType::__is_field and (not FieldType::__is_variable)
    static qsizetype estimateFieldSize(const FieldType &)
    {
        return sizeof(typename FieldType::data_type);
    }

public:
    template<typename T>
    static QByteArray generate(T &t)
    {
        CTRACK;

        constexpr auto fields = T::fields;

        // Pre-size the output to avoid QByteArray growth reallocations.
        // Base + variable field sizes.
        qsizetype estimate = sizeof(quint16); // type prefix
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((estimate += estimateFieldSize(t.*std::get<Is>(fields))), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});

        QByteArray output;
        output.reserve(estimate);

        QDataStream stream(&output, QIODeviceBase::WriteOnly);

        stream.setByteOrder(QDataStream::BigEndian);

        stream << static_cast<quint16>(T::type);

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((updateSizeIfNeeded(t.*std::get<Is>(fields))), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((writeField(stream, t.*std::get<Is>(fields))), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((writeVariableField(stream, t.*std::get<Is>(fields))), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});

        return output;
    }
};

template<
    typename Receiver,
    const int ErrorLimit = 2,
    std::size_t MaxVariableSize = 8 * 1024 * 1024,
    bool DebugParsed = false>
class Parser
{
    using PacketTypes = typename details::variant_types<PacketVariant>::types;

    enum ParseOutput {
        False,
        True,
        Continue,
    };

    template<typename PacketType, size_t Index>
    using FieldType =
        typename details::member_pointer_traits<std::tuple_element_t<Index, decltype(PacketType::fields)>>::member_type;

    QByteArray m_array{};
    Type m_state = Type::None;
    PacketVariant m_current{};
    PacketTypes m_bounds{};
    Receiver &m_receiver;
    std::set<Type> m_waitsFor{};
    std::set<Type> m_disabled{};
    size_t m_parsed = 0;
    size_t m_variablesParsed = 0;
    int m_errorCount = 0;
    bool m_discardCurrent = false;

    inline void resetToResync()
    {
        m_state = Type::None;
        m_parsed = 0;
        m_variablesParsed = 0;
        m_current = PacketVariant{};
        m_discardCurrent = false;
    }

    template<typename PacketType, size_t Index>
        requires FieldType<PacketType, Index>::__is_variable and FieldType<PacketType, Index>::__has_limits
    inline ParseOutput read()
    {
        using Field = FieldType<PacketType, Index>;
        auto &packet = std::get<PacketType>(m_current);
        auto &field = packet.*std::get<Index>(std::get<PacketType>(m_current).fields);

        if (m_array.size() >= static_cast<qsizetype>(sizeof(typename Field::size_type))) {
            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> field.size;

            m_array.remove(0, sizeof(field.size));

            if (field.size > MaxVariableSize) [[unlikely]] {
                return False;
            }

            return (field.size <= field.max && field.size >= field.min) ? True : False;
        }

        return Continue;
    }

    template<typename PacketType, size_t Index>
        requires(not FieldType<PacketType, Index>::__is_variable) and FieldType<PacketType, Index>::__has_limits
    inline ParseOutput read()
    {
        using Field = FieldType<PacketType, Index>;
        auto &packet = std::get<PacketType>(m_current);
        auto &field = packet.*std::get<Index>(std::get<PacketType>(m_current).fields);

        if (m_array.size() >= static_cast<qsizetype>(sizeof(typename Field::data_type))) {
            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> field.data;

            m_array.remove(0, sizeof(field.data));

            return (field.data <= field.max && field.data >= field.min) ? True : False;
        }

        return Continue;
    }

    template<typename PacketType, size_t Index>
        requires(not FieldType<PacketType, Index>::__is_variable) and (not FieldType<PacketType, Index>::__has_limits)
    inline ParseOutput read()
    {
        using Field = FieldType<PacketType, Index>;
        auto &packet = std::get<PacketType>(m_current);
        auto &field = packet.*std::get<Index>(std::get<PacketType>(m_current).fields);

        if (m_array.size() >= static_cast<qsizetype>(sizeof(typename Field::data_type))) {
            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> field.data;

            m_array.remove(0, sizeof(field.data));

            return True;
        }

        return Continue;
    }

    template<typename FieldType, size_t = 0>
    inline ParseOutput read(FieldType &field)
    {
        if (m_array.size() >= static_cast<qsizetype>(sizeof(FieldType))) {
            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> field;

            m_array.remove(0, sizeof(FieldType));

            return True;
        }

        return Continue;
    }

    template<typename PacketType, size_t Index>
        requires FieldType<PacketType, Index>::__is_field and FieldType<PacketType, Index>::__is_variable
    inline ParseOutput readVariable()
    {
        auto &packet = std::get<PacketType>(m_current);
        auto &field = packet.*std::get<Index>(std::get<PacketType>(m_current).fields);

        const auto size = static_cast<qsizetype>(field.size);
        if (m_array.size() >= size) {
            if (m_discardCurrent) {
                m_array.remove(0, size);
            } else {
                QDataStream stream(m_array);
                stream.setByteOrder(QDataStream::BigEndian);

                field.data.resize(size);
                stream.readRawData(field.data.data(), static_cast<qint64>(size));
                m_array.remove(0, size);
            }

            return True;
        }

        return Continue;
    }

    template<typename PacketType, size_t Index>
        requires(not(FieldType<PacketType, Index>::__is_field and FieldType<PacketType, Index>::__is_variable))
    inline constexpr ParseOutput readVariable()
    {
        return True;
    }

    template<typename T>
    inline ParseOutput instanceParsing()
    {
        CTRACK;

        if (m_parsed == 0) {
            m_current = std::get<details::Index<T, decltype(m_bounds)>::value>(m_bounds);
        }

        static constexpr size_t fieldCount = std::tuple_size_v<decltype(T::fields)>;
        static constexpr auto fieldTable = [] {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return std::array<ParseOutput (Parser::*)(), fieldCount>{ &Parser::read<T, Is>... };
            }(std::make_index_sequence<fieldCount>{});
        }();
        static constexpr auto variableFieldTable = [] {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return std::array<ParseOutput (Parser::*)(), fieldCount>{ &Parser::readVariable<T, Is>... };
            }(std::make_index_sequence<fieldCount>{});
        }();

        while (m_parsed < fieldCount) {
            const auto result = (this->*fieldTable[m_parsed])();
            switch (result) {
            case True:
                ++m_parsed;
                continue;
            default:
                return result;
            }
        }

        while (m_variablesParsed < fieldCount) {
            const auto result = (this->*variableFieldTable[m_variablesParsed])();
            switch (result) {
            case True:
                ++m_variablesParsed;
                continue;
            default:
                return result;
            }
        }

        if (m_discardCurrent) {
            m_discardCurrent = false;
        } else if constexpr (details::has_processPacket<Receiver, T>::value) {
            m_receiver.processPacket(std::as_const(std::get<T>(m_current)));
        }

        if constexpr (DebugParsed) {
            qDebug() << "Parsed" << static_cast<int>(m_state);
        }

        return True;
    }

    ParseOutput dispatch(const Type state)
    {
        CTRACK;

        using Types = typename details::variant_types<PacketVariant>::types;
        constexpr size_t N = std::tuple_size_v<Types>;
        constexpr size_t TABLE_SIZE = static_cast<size_t>(Type::MAXIMUM) + 1;

        static constexpr auto table = [] {
            std::array<ParseOutput (Parser::*)(), TABLE_SIZE> arr{};
            arr.fill(nullptr);

            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((arr[static_cast<size_t>(std::tuple_element_t<Is, Types>::type)]
                     = &Parser::instanceParsing<std::tuple_element_t<Is, Types>>),
                    ...);
            }(std::make_index_sequence<N>{});

            return arr;
        }();

        const auto idx = static_cast<size_t>(state);
        if (idx < TABLE_SIZE && table[idx]) {
            return (this->*table[idx])();
        }

        return False;
    }

    template<typename Class, typename Returns>
        requires requires(Returns &r) {
            r.min = r.min;
            r.max = r.max;
            r.setBounds(r.min, r.max);
        }
    inline void setBoundsHelper(Returns Class::*member, const auto min, const auto max)
    {
        auto &field = std::get<details::Index<Class, PacketTypes>::value>(m_bounds).*member;

        field.setBounds(min, max);
    }

public:
    explicit Parser(Receiver &receiver)
        : m_receiver(receiver)
    {}

    inline void pushData(const QByteArray &additional)
    {
        m_array.append(additional);
        parse();
    }

    void clear()
    {
        m_array.clear();
        resetToResync();
        m_errorCount = 0;
    }

    inline void waitFor(const Type t)
    {
        m_waitsFor.insert(t);
    }

    inline void disable(const Type t)
    {
        m_disabled.insert(t);
    }
    inline void enable(const Type t)
    {
        m_disabled.erase(t);
    }
    inline void clearRules()
    {
        m_disabled.clear();
        m_waitsFor.clear();
    }

    void parse()
    {
        while (true) {
            CTRACK;

            switch (m_state) {
            case Type::None: {
                if (m_array.size() < static_cast<qsizetype>(sizeof(quint16))) {
                    return;
                }

                quint16 type = static_cast<quint16>(Type::None);
                if (read<quint16>(type) != True) {
                    return;
                }

                if ((type > static_cast<quint16>(Type::UPPER) || type < static_cast<quint16>(Type::LOWER)))
                    [[unlikely]] {
                    if constexpr (details::has_packetErrors<Receiver>::value) {
                        ++m_errorCount;
                        if (m_errorCount > ErrorLimit) {
                            m_errorCount = 0;
                            m_receiver.onPacketErrors();
                        }
                    }

                    continue;
                }

                const auto converted = static_cast<Type>(type);

                m_discardCurrent = false;
                if (!m_waitsFor.empty()) [[unlikely]] {
                    if (m_waitsFor.contains(converted)) {
                        m_waitsFor.erase(converted);
                    } else {
                        m_discardCurrent = true;
                    }
                }

                if (m_disabled.contains(converted)) {
                    m_discardCurrent = true;
                }

                m_state = converted;
                m_parsed = 0;
                m_variablesParsed = 0;
                [[fallthrough]];
            }
            default: {
                switch (dispatch(m_state)) {
                case Continue: {
                    return;
                }
                case False: {
                    if constexpr (details::has_packetErrors<Receiver>::value) {
                        ++m_errorCount;
                        if (m_errorCount > ErrorLimit) {
                            m_errorCount = 0;
                            m_receiver.onPacketErrors();
                        }
                    }

                    resetToResync();
                    break;
                }
                case True: {
                    resetToResync();
                    break;
                }
                }

                break;
            }
            }
        }
    }

    template<auto ptr>
    inline void setBounds(const auto min, const auto max)
    {
        setBoundsHelper(ptr, min, max);
    }
};

// Limit is the maximum number of TS queues that can be handled simultaneously.
template<
    typename OrderType = Ordering,
    typename TsType = Timestamp,
    const TsType MaxTimestamps = 5,
    const std::size_t PacketsLimit = 200>
class JitterBuffer
{
    struct ChunkQueue
    {
        std::vector<std::optional<QByteArray>> chunks{};

        inline bool isComplete() const
        {
            return !chunks.empty()
                && std::none_of(chunks.cbegin(), chunks.cend(), [](const auto &opt) { return !opt.has_value(); });
        }

        inline QByteArray assemble() const
        {
            CTRACK;

            const auto total = std::accumulate(
                chunks.cbegin(),
                chunks.cend(),
                qsizetype(0),
                [](qsizetype sum, const auto &opt) { return sum + (opt ? opt->size() : 0); }
            );

            QByteArray out;
            out.reserve(total);
            for (const auto &opt : chunks) {
                if (opt) {
                    out += *opt;
                }
            }

            return out;
        }
    };

    struct Entry
    {
        TsType ts{};
        ChunkQueue queue{};
    };

    std::vector<Entry> m_entries{};

public:
    // Insert a chunk for a given timestamp and order.
    // `totalChunks` is the total number of chunks for this timestamp (must be > 0).
    void pushChunk(const TsType ts, const OrderType order, const OrderType totalChunks, QByteArray data)
    {
        if (totalChunks == 0 || order >= totalChunks) {
            return;
        }

        CTRACK;

        auto it = std::find_if(m_entries.begin(), m_entries.end(), [ts](const Entry &e) { return e.ts == ts; });
        if (it == m_entries.end()) {
            while (m_entries.size() >= MaxTimestamps) {
                m_entries.erase(m_entries.begin()); // remove oldest
            }

            m_entries.push_back(Entry{ ts, ChunkQueue{} });
            it = std::prev(m_entries.end());
        }

        auto &queue = it->queue;

        // Guard against a stale queue whose shape doesn't match the incoming
        // frame (e.g. a reused ts after wrap).
        if (!queue.chunks.empty() && queue.chunks.size() != totalChunks) {
            queue.chunks.clear();
        }

        // Ensure the chunks vector is large enough to hold all orders.
        if (queue.chunks.size() < totalChunks) {
            queue.chunks.resize(totalChunks);
        }

        // Store the chunk if the slot is empty (ignore duplicates).
        if (order < queue.chunks.size() && !queue.chunks[order].has_value()) {
            queue.chunks[order] = std::move(data);
        }
    }

    // Attempt to retrieve a complete packet, oldest first.
    // Returns the assembled QByteArray if a complete timestamp is found,
    // otherwise std::nullopt.
    inline std::optional<QByteArray> pullComplete(bool &outDroppedAny)
    {
        CTRACK;

        if (outDroppedAny) {
            outDroppedAny = false;
        }

        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->queue.isComplete()) {
                const QByteArray assembled = it->queue.assemble();
                if (outDroppedAny && it != m_entries.begin()) {
                    outDroppedAny = true;
                }

                // Remove this timestamp and all older ones (they are no longer needed).
                m_entries.erase(m_entries.begin(), std::next(it));
                return assembled;
            }
        }

        return std::nullopt;
    }
    inline std::optional<QByteArray> pullComplete()
    {
        CTRACK;

        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->queue.isComplete()) {
                const QByteArray assembled = it->queue.assemble();
                // Remove this timestamp and all older ones (they are no longer needed).
                m_entries.erase(m_entries.begin(), std::next(it));
                return assembled;
            }
        }

        return std::nullopt;
    }

    // Clear all stored data.
    inline void clear()
    {
        m_entries.clear();
    }

    // Optional: check if a timestamp is already complete (for debugging).
    inline bool isComplete(const TsType ts) const
    {
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [ts](const Entry &e) { return e.ts == ts; });
        return it != m_entries.end() && it->queue.isComplete();
    }
};

} // namespace Packets

#endif // PARSER_H
