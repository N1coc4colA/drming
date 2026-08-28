#ifndef PARSER_H
#define PARSER_H

#include <QByteArray>
#include <QDataStream>
#include <QDebug>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace Packets {

using Ordering = quint32;
using Timestamp = qint16;
using Keystamp = qint16;

enum class Type : quint16 {
    None = 0,
    HeartBeat,
    Reinit,

    RequestKey,
    KeyUpdate,

    ServerImage,
    RequestClientResolution,
    ClientResolution,
    ServerBrightness,
    ServerStream,

    MINIMUM = HeartBeat,
    MAXIMUM = ServerStream,

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
{};

template<typename Receiver, typename Packet>
struct has_processPacket<Receiver, Packet, std::void_t<decltype(std::declval<Receiver>().processPacket(std::declval<const Packet &>()))>>
    : std::true_type
{};

template<typename Receiver, typename = void>
struct has_packetErrors : std::false_type
{};

template<typename Receiver>
struct has_packetErrors<Receiver, std::void_t<decltype(std::declval<Receiver>().packetErrors())>> : std::true_type
{};

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

template<typename T>
struct Field
{
    using data_type = T;

    static constexpr bool __is_field = true;
    static constexpr bool __is_variable = false;
    static constexpr bool __has_limits = false;

    T data{};

    inline T &operator->() { return data; }
    inline const T &operator->() const { return data; }
};

template<typename T, typename SizeType>
struct SizedField : public Field<T>
{
    using size_type = SizeType;

    static constexpr bool __is_variable = true;

    SizeType size{};
};

template<typename T, typename LimitType, LimitType Min, LimitType Max>
struct FixedField : Field<T>
{
    static constexpr bool __has_limits = true;

    static constexpr LimitType min = Min;
    static constexpr LimitType max = Max;
};

template<typename T, typename LimitType, LimitType DefaultMin = 0, LimitType DefaultMax = std::numeric_limits<LimitType>::max()>
struct BoundField : public Field<T>
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

template<typename T, typename LimitType, LimitType Min, LimitType Max>
struct VariableFixedField : SizedField<T, LimitType>
{
    static constexpr bool __has_limits = true;

    static constexpr LimitType min = Min;
    static constexpr LimitType max = Max;
};

template<typename T, typename LimitType, LimitType DefaultMin = 0, LimitType DefaultMax = std::numeric_limits<LimitType>::max()>
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
{};

template<typename EnumType>
    requires requires() {
        std::is_enum_v<EnumType>;
        EnumType::UPPER;
        EnumType::LOWER;
        std::is_same_v<typename EnumType::UPPER, EnumType>;
        std::is_same_v<typename EnumType::LOWER, EnumType>;
    }
struct BoundEnumField : public BoundField<EnumType, EnumType, EnumType::LOWER, EnumType::UPPER>
{};

template<typename NumericType, NumericType Min = std::numeric_limits<NumericType>::min(), NumericType Max = std::numeric_limits<NumericType>::max()>
struct NumericField : FixedField<NumericType, NumericType, Min, Max>
{};

template<typename NumericType,
         NumericType DefaultMin = std::numeric_limits<NumericType>::min(),
         NumericType DefaultMax = std::numeric_limits<NumericType>::max()>
struct BoundNumericField : BoundField<NumericType, NumericType, DefaultMin, DefaultMax>
{};

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

    static constexpr auto fields = std::tuple{&KeyUpdate::keyStamp, &KeyUpdate::key};
};

struct ServerImage
{
    static constexpr auto type = Type::ServerImage;

    VariableFixedField<QByteArray, quint64, 3, 4> format;
    VariableBoundField<QByteArray, quint64, 1, 1920 * 1080> data;

    static constexpr auto fields = std::tuple{&ServerImage::format, &ServerImage::data};
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

    static constexpr auto fields = std::tuple{&ClientResolution::width, &ClientResolution::height};
};

struct ServerBrightness
{
    static constexpr auto type = Type::ServerBrightness;

    NumericField<std::int16_t, 0, 10> brightness;

    static constexpr auto fields = std::tuple{&ServerBrightness::brightness};
};

struct ServerStream
{
    static constexpr auto type = Type::ServerStream;
    VariableBoundField<QByteArray, quint64, 1> data;

    static constexpr auto fields = std::tuple{&ServerStream::data};
};

using PacketVariant
    = std::variant<HeartBeat, Reinit, RequestKey, KeyUpdate, ServerImage, RequestClientResolution, ClientResolution, ServerBrightness, ServerStream>;

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

public:
    template<typename T>
    static QByteArray generate(T &t)
    {
        constexpr auto fields = T::fields;

        QByteArray output;
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

template<typename Receiver, const int ErrorLimit = 64 * 1024 * 1024, std::size_t MaxVariableSize = 1024 * 1024>
class Parser
{
    using PacketTypes = typename details::variant_types<PacketVariant>::types;

    enum ParseOutput {
        False,
        True,
        Continue,
    };

    template<typename PacketType, size_t Index>
    using FieldType = typename details::member_pointer_traits<std::tuple_element_t<Index, decltype(PacketType::fields)>>::member_type;

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

    inline void resetToResync()
    {
        m_state = Type::None;
        m_parsed = 0;
        m_variablesParsed = 0;
        m_current = PacketVariant{};
    }

    // If it is variable and has limits, so we need to get size then bound check.
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

    // Not bounded, so just grab the size, the data will be retrieved in readVariable.
    template<typename PacketType, size_t Index>
        requires FieldType<PacketType, Index>::__is_variable and (not FieldType<PacketType, Index>::__has_limits)
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

            return True;
        }

        return Continue;
    }

    // A simply bounded field.
    template<typename PacketType, size_t Index>
        requires(not FieldType<PacketType, Index>::__is_variable) and FieldType<PacketType, Index>::__has_limits
    inline ParseOutput read()
    {
        using Field = FieldType<PacketType, Index>;
        auto &packet = std::get<PacketType>(m_current);

        if (m_array.size() >= static_cast<qsizetype>(sizeof(typename Field::data_type))) {
            auto &field = packet.*std::get<Index>(std::get<PacketType>(m_current).fields);

            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> field.data;

            m_array.remove(0, sizeof(field.data));

            return (field.data <= field.max && field.data >= field.min) ? True : False;
        }

        return Continue;
    }

    // Simply a declared field.
    template<typename PacketType, size_t Index>
        requires(not FieldType<PacketType, Index>::__is_variable) and (not FieldType<PacketType, Index>::__has_limits)
    inline ParseOutput read()
    {
        using Field = FieldType<PacketType, Index>;
        auto &packet = std::get<PacketType>(m_current);

        if (m_array.size() >= static_cast<qsizetype>(sizeof(typename Field::data_type))) {
            auto &field = packet.*std::get<Index>(std::get<PacketType>(m_current).fields);

            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> field.data;

            m_array.remove(0, sizeof(field.data));

            return True;
        }

        return Continue;
    }

    // Just normally read the data.
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
            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);

            field.data.resize(static_cast<qsizetype>(size));
            stream.readRawData(field.data.data(), static_cast<qint64>(size));
            m_array.remove(0, size);

            return True;
        }

        return Continue;
    }

    template<typename PacketType, size_t Index>
    inline constexpr ParseOutput readVariable()
    {
        return True;
    }

    template<typename T>
    inline ParseOutput instanceParsing()
    {
        if (m_parsed == 0) {
            m_current = std::get<details::Index<T, decltype(m_bounds)>::value>(m_bounds);
        }

        static constexpr size_t fieldCount = std::tuple_size_v<decltype(T::fields)>;
        static constexpr auto fieldTable = [] {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return std::array<ParseOutput (Parser::*)(), fieldCount>{&Parser::read<T, Is>...};
            }(std::make_index_sequence<fieldCount>{});
        }();
        static constexpr auto variableFieldTable = [] {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return std::array<ParseOutput (Parser::*)(), fieldCount>{&Parser::readVariable<T, Is>...};
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

        // Call processPacket only if the receiver has an appropriate overload
        if constexpr (details::has_processPacket<Receiver, T>::value) {
            m_receiver.processPacket(std::as_const(std::get<T>(m_current)));
        }

        return True;
    }

    ParseOutput dispatch(const Type state)
    {
        using Types = typename details::variant_types<PacketVariant>::types;
        constexpr size_t N = std::tuple_size_v<Types>;
        constexpr size_t TABLE_SIZE = static_cast<size_t>(Type::MAXIMUM) + 1;

        static constexpr auto table = [] {
            std::array<ParseOutput (Parser::*)(), TABLE_SIZE> arr{};
            arr.fill(nullptr);

            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((arr[static_cast<size_t>(std::tuple_element_t<Is, Types>::type)] = &Parser::instanceParsing<std::tuple_element_t<Is, Types>>), ...);
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

    inline void addData(const QByteArray &additional)
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

    inline void waitFor(const Type t) { m_waitsFor.insert(t); }

    inline void disable(const Type t) { m_disabled.insert(t); }
    inline void enable(const Type t) { m_disabled.erase(t); }
    inline void clearRules()
    {
        m_disabled.clear();
        m_waitsFor.clear();
    }

    void parse()
    {
        while (true) {
            switch (m_state) {
            case Type::None: {
                if (m_array.size() < static_cast<qsizetype>(sizeof(quint16))) {
                    return;
                }

                quint16 type = static_cast<quint16>(Type::None);
                if (read<quint16>(type) != True) {
                    // read for non Field-derived types always return True (read success) or Continue (need more data).
                    return;
                }

                if ((type > static_cast<quint16>(Type::UPPER) || type < static_cast<quint16>(Type::LOWER))) [[unlikely]] {
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
                if (!m_waitsFor.empty()) [[unlikely]] {
                    if (!m_waitsFor.contains(converted)) {
                        continue;
                    } else {
                        m_waitsFor.erase(converted);
                    }
                }

                // Just walk by if disabled.
                if (m_disabled.contains(converted)) {
                    continue;
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

                    [[fallthrough]];
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
template<typename OrderType = Ordering, typename TsType = Timestamp, TsType MaxTimestamps = 5, std::size_t PacketsLimit = 200>
class JitterBuffer
{
    struct ChunkQueue
    {
        std::vector<std::optional<QByteArray>> chunks{};

        inline bool isComplete() const
        {
            return std::none_of(chunks.cbegin(), chunks.cend(), [](const auto &opt) { return !opt.has_value(); });
        }

        inline QByteArray assemble() const
        {
            const auto total = std::accumulate(chunks.cbegin(), chunks.cend(), qsizetype(0), [](qsizetype sum, const auto &opt) {
                return sum + opt->size();
            });

            QByteArray out;
            out.reserve(total);
            for (const auto &opt : chunks) {
                out += *opt;
            }

            return out;
        }
    };

    std::map<TsType, ChunkQueue> m_data{}; // sorted by timestamp, oldest first

public:
    // Insert a chunk for a given timestamp and order.
    // `totalChunks` is the total number of chunks for this timestamp (must be > 0).
    void pushChunk(const TsType ts, const OrderType order, const OrderType totalChunks, const QByteArray data)
    {
        // If we already have MaxTimestamps timestamps and this one is not present,
        // evict the oldest one to make room.
        auto it = m_data.find(ts);
        if (it == m_data.end()) {
            while (m_data.size() >= MaxTimestamps) {
                m_data.erase(m_data.begin()); // remove oldest
            }

            it = m_data.emplace(ts, ChunkQueue{}).first;
        }

        auto &queue = it->second;
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
    inline std::optional<QByteArray> pullComplete()
    {
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            auto &queue = it->second;
            if (queue.isComplete()) {
                QByteArray assembled = queue.assemble();
                // Remove this timestamp and all older ones (they are no longer needed).
                m_data.erase(m_data.begin(), std::next(it));
                return assembled;
            }
        }

        return std::nullopt;
    }

    // Clear all stored data.
    inline void clear() { m_data.clear(); }

    // Optional: check if a timestamp is already complete (for debugging).
    inline bool isComplete(const TsType ts) const
    {
        const auto it = m_data.find(ts);

        return it != m_data.end() && it->second.isComplete();
    }
};

} // namespace Packets

#endif // PARSER_H
