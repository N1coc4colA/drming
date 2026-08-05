#ifndef PARSER_H
#define PARSER_H

#include <QDataStream>
#include <QDebug>
#include <QFloat16>
#include <qtypes.h>

#include <expected>
#include <type_traits>
#include <array>
#include <tuple>
#include <variant>
#include <limits>
#include <utility>      // for std::as_const

namespace Packets {

// -----------------------------------------------------------------------------
// Field descriptors
// -----------------------------------------------------------------------------

template<typename Class, typename T>
struct FixedField
{
    using type = T;
    T Class::*ptr;
};

template<typename Class,
         typename SizeType,
         typename DataType,
         size_t Index,
         SizeType DefaultMin = 0,
         SizeType DefaultMax = std::numeric_limits<SizeType>::max()>
struct SizedField
{
    using size_type = SizeType;
    using data_type = DataType;
    SizeType Class::*sizePtr;
    DataType Class::*dataPtr;
    static constexpr size_t index = Index;
    static constexpr SizeType defaultMin = DefaultMin;
    static constexpr SizeType defaultMax = DefaultMax;
};

// -----------------------------------------------------------------------------
// Traits
// -----------------------------------------------------------------------------

template<typename> struct is_fixed_field : std::false_type {};
template<typename C, typename T>
struct is_fixed_field<FixedField<C, T>> : std::true_type {};

template<typename> struct is_sized_field : std::false_type {};
template<typename C, typename S, typename D, size_t Idx, S Min, S Max>
struct is_sized_field<SizedField<C, S, D, Idx, Min, Max>> : std::true_type {};

template<typename F>
constexpr bool is_fixed_field_v = is_fixed_field<F>::value;
template<typename F>
constexpr bool is_sized_field_v = is_sized_field<F>::value;

template<typename> struct always_false : std::false_type {};

// Count sized fields
template<typename T>
struct count_sized_fields;

template<typename... Fields>
struct count_sized_fields<std::tuple<Fields...>>
{
    static constexpr size_t value = (is_sized_field_v<Fields> + ... + 0);
};

template<typename... Fields>
struct count_sized_fields<const std::tuple<Fields...>>
{
    static constexpr size_t value = count_sized_fields<std::tuple<Fields...>>::value;
};

template<typename T>
using packet_bounds_type = std::array<std::pair<qsizetype, qsizetype>,
                                      count_sized_fields<decltype(T::fields)>::value>;

// Helper: extract class from a member pointer
template<typename> struct member_pointer_class;

template<typename Class, typename Member>
struct member_pointer_class<Member Class::*>
{
    using type = Class;
};

// Helper: find the index of a sized field whose dataPtr matches a given pointer
template<typename FieldsTuple, typename DataPtr>
struct find_sized_field_index;

template<typename... Fields, typename DataPtr>
struct find_sized_field_index<std::tuple<Fields...>, DataPtr>
{
private:
    template<size_t I>
    static constexpr size_t check()
    {
        if constexpr (I == sizeof...(Fields)) {
            return static_cast<size_t>(-1);
        } else {
            using Field = std::tuple_element_t<I, std::tuple<Fields...>>;
            if constexpr (is_sized_field_v<Field>) {
                if constexpr (std::is_same_v<decltype(Field::dataPtr), DataPtr>) {
                    return I;
                } else {
                    return check<I + 1>();
                }
            } else {
                return check<I + 1>();
            }
        }
    }
public:
    static constexpr size_t value = check<0>();
};

// -----------------------------------------------------------------------------
// Packet types
// -----------------------------------------------------------------------------

enum class Type : quint16 {
    None = 0,
    HeartBeat,
    Reinit,
    ServerImage,
    ClientResolution,
    ServerBrightness,
    ServerStream,

    MINIMUM = HeartBeat,
    MAXIMUM = ServerStream,

    LOWER = HeartBeat,
    UPPER = MAXIMUM
};

// -----------------------------------------------------------------------------
// Packet definitions
// -----------------------------------------------------------------------------

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

struct ServerImage
{
    static constexpr auto type = Type::ServerImage;
    qsizetype formatSize = 0;
    qsizetype imageSize = 0;
    QByteArray format;
    QByteArray data;

    static constexpr auto fields = std::tuple{
        FixedField<ServerImage, qsizetype>{&ServerImage::formatSize},
        FixedField<ServerImage, qsizetype>{&ServerImage::imageSize},
        SizedField<ServerImage, qsizetype, QByteArray, 0>{&ServerImage::formatSize, &ServerImage::format},
        SizedField<ServerImage, qsizetype, QByteArray, 1>{&ServerImage::imageSize, &ServerImage::data}
    };
};

struct ClientResolution
{
    static constexpr auto type = Type::ClientResolution;
    quint32 width = 0;
    quint32 height = 0;

    static constexpr auto fields = std::tuple{
        FixedField<ClientResolution, quint32>{&ClientResolution::width},
        FixedField<ClientResolution, quint32>{&ClientResolution::height}
    };
};

struct ServerBrightness
{
    static constexpr auto type = Type::ServerBrightness;
    qfloat16 brightness;

    static constexpr auto fields = std::tuple{
        FixedField<ServerBrightness, qfloat16>{&ServerBrightness::brightness}
    };
};

struct ServerStream
{
    static constexpr auto type = Type::ServerStream;
    qsizetype frameSize = 0;
    QByteArray data;

    static constexpr auto fields = std::tuple{
        FixedField<ServerStream, qsizetype>{&ServerStream::frameSize},
        SizedField<ServerStream, qsizetype, QByteArray, 0>{&ServerStream::frameSize, &ServerStream::data}
    };
};

using PacketVariant = std::variant<HeartBeat, Reinit, ServerImage,
                                   ClientResolution, ServerBrightness, ServerStream>;

// -----------------------------------------------------------------------------
// Utility: index of a type in a tuple
// -----------------------------------------------------------------------------

template<typename T, typename... Ts>
struct PacketIndex_counter;

template<typename T, typename First, typename... Rest>
struct PacketIndex_counter<T, First, Rest...>
{
private:
    static constexpr std::size_t next = PacketIndex_counter<T, Rest...>::value;

public:
    static constexpr std::size_t value = std::is_same_v<T, First> ? 0
                                      : (next == static_cast<std::size_t>(-1) ? next : next + 1);
};

template<typename T>
struct PacketIndex_counter<T>
{
    static constexpr std::size_t value = static_cast<std::size_t>(-1);
};

template<typename T, typename Tuple>
struct type_index_in_tuple;

template<typename T, typename... Ts>
struct type_index_in_tuple<T, std::tuple<Ts...>>
{
    static constexpr size_t value = PacketIndex_counter<T, Ts...>::value;
};

// -----------------------------------------------------------------------------
// Validator
// -----------------------------------------------------------------------------

template<typename T>
struct Validator
{
    template<typename O>
    static constexpr std::expected<O, bool> validateTo(const T &t)
    {
        return static_cast<O>(t);
    }

    template<typename O>
    static constexpr std::expected<T, bool> validateFrom(const O &o)
    {
        return static_cast<T>(o);
    }
};

template<>
struct Validator<Type>
{
    template<typename O>
    static constexpr std::expected<O, bool> validateTo(const Type &t)
    {
        return static_cast<O>(t);
    }

    template<typename O>
    static constexpr std::expected<Type, bool> validateFrom(const O &o)
    {
        const auto lower = static_cast<O>(Type::LOWER);
        const auto upper = static_cast<O>(Type::UPPER);
        if (o >= lower && o <= upper) [[likely]] {
            return static_cast<Type>(o);
        }
        qInfo() << '[' << lower << ';' << upper << ']' << o;
        return std::unexpected(true);
    }
};

// -----------------------------------------------------------------------------
// Inverse mapping Type -> packet struct
// -----------------------------------------------------------------------------

template<Type Wanted, typename... Ts>
struct find_packet;

template<Type Wanted, typename Head, typename... Tail>
struct find_packet<Wanted, Head, Tail...>
{
    using type = std::conditional_t<Head::type == Wanted, Head,
                                    typename find_packet<Wanted, Tail...>::type>;
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

template<typename V, Type Wanted>
using TypeToPacket = variant_types<V>::template find<Wanted>;

// -----------------------------------------------------------------------------
// Trait: detect if Receiver has a processPacket overload for a given packet type
// -----------------------------------------------------------------------------
template<typename Receiver, typename Packet, typename = void>
struct has_processPacket : std::false_type {};

template<typename Receiver, typename Packet>
struct has_processPacket<Receiver, Packet,
    std::void_t<decltype(std::declval<Receiver>().processPacket(std::declval<const Packet&>()))>>
    : std::true_type {};

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------

template<typename Receiver, const int ErrorLimit = 5>
class Parser
{
    using PacketTypes = typename variant_types<PacketVariant>::types;

    enum ParseOutput : bool {
        False = false,
        True = true,
    };

    // -------------------------------------------------------------------------
    // Helper to build bounds tuple
    // -------------------------------------------------------------------------
    template<typename Tuple, typename = std::make_index_sequence<std::tuple_size_v<Tuple>>>
    struct BoundsTuple;

    template<typename Tuple, size_t... Is>
    struct BoundsTuple<Tuple, std::index_sequence<Is...>>
    {
        using type = std::tuple<packet_bounds_type<std::tuple_element_t<Is, Tuple>>...>;
    };

    using BoundsTupleType = typename BoundsTuple<PacketTypes>::type;
    BoundsTupleType m_bounds;

    // -------------------------------------------------------------------------
    // Internal state
    // -------------------------------------------------------------------------
    QByteArray m_array{};
    Type m_state = Type::None;
    PacketVariant m_current{};
    Receiver &m_receiver;
    size_t m_parsed = 0;
    int m_errorCount = 0;

    // -------------------------------------------------------------------------
    // Read a value of type T from the stream (with optional underlying type)
    // -------------------------------------------------------------------------
    template<typename T, typename Underlying = T>
    std::expected<T, bool> read()
    {
        if (m_array.size() >= static_cast<qsizetype>(sizeof(Underlying))) {
            Underlying tmp;
            QDataStream stream(m_array);
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> tmp;
            return Validator<T>::template validateFrom<Underlying>(tmp);
        }
        return std::unexpected(false);
    }

    // -------------------------------------------------------------------------
    // Process a single field (fixed or sized)
    // -------------------------------------------------------------------------
    template<typename T, size_t I>
    bool processField()
    {
        auto &packet = std::get<T>(m_current);
        constexpr auto &field = std::get<I>(T::fields);

        if constexpr (is_fixed_field_v<std::decay_t<decltype(field)>>) {
            using MemberType = typename std::decay_t<decltype(field)>::type;
            auto result = read<MemberType>();
            if (!result) {
                if (result.error()) {
                    m_array.remove(0, sizeof(MemberType));
                    ++m_errorCount;
                    if (m_errorCount > ErrorLimit) {
                        m_errorCount = 0;
                        m_receiver.onPacketErrors();
                    }
                }
                return false;
            }
            packet.*field.ptr = *result;
            m_array.remove(0, sizeof(MemberType));
            ++m_parsed;
            return true;
        }
        else if constexpr (is_sized_field_v<std::decay_t<decltype(field)>>) {
            auto size = packet.*field.sizePtr;

            constexpr size_t packetIdx = type_index_in_tuple<T, PacketTypes>::value;
            auto &boundsArray = std::get<packetIdx>(m_bounds);
            constexpr size_t fieldIdx = std::decay_t<decltype(field)>::index;
            auto &limits = boundsArray[fieldIdx];

            if (size < static_cast<decltype(size)>(limits.first) ||
                size > static_cast<decltype(size)>(limits.second)) {
                if (!m_array.isEmpty())
                    m_array.remove(0, 1);
                ++m_errorCount;
                if (m_errorCount > ErrorLimit) {
                    m_errorCount = 0;
                    m_receiver.onPacketErrors();
                }
                return false;
            }

            if (m_array.size() < static_cast<qsizetype>(size))
                return false;

            packet.*field.dataPtr = m_array.first(size);
            m_array.remove(0, size);
            ++m_parsed;
            return true;
        }
        else {
            static_assert(always_false<T>::value, "unknown field type");
            return false;
        }
    }

    // -------------------------------------------------------------------------
    // Parse an entire packet of type T
    // -------------------------------------------------------------------------
    template<typename T>
    bool instanceParsing()
    {
        if (m_parsed == 0) {
            m_current = T{};
        }

        static constexpr size_t fieldCount = std::tuple_size_v<decltype(T::fields)>;
        static constexpr auto fieldTable = [] {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return std::array<bool (Parser::*)(), fieldCount>{
                    &Parser::processField<T, Is>...
                };
            }(std::make_index_sequence<fieldCount>{});
        }();

        while (m_parsed < fieldCount) {
            bool ok = (this->*fieldTable[m_parsed])();
            if (!ok)
                return false;
        }

        // Call processPacket only if the receiver has an appropriate overload
        if constexpr (has_processPacket<Receiver, T>::value) {
            m_receiver.processPacket(std::as_const(std::get<T>(m_current)));
        }

        m_state = Type::None;
        m_parsed = 0;
        m_errorCount = 0;
        return true;
    }

    // -------------------------------------------------------------------------
    // Dispatch to the correct instanceParsing based on the packet type
    // -------------------------------------------------------------------------
    ParseOutput dispatch(Type state)
    {
        using Types = typename variant_types<PacketVariant>::types;
        constexpr size_t N = std::tuple_size_v<Types>;
        constexpr size_t TABLE_SIZE = static_cast<size_t>(Type::MAXIMUM) + 1;

        static constexpr auto table = [] {
            std::array<bool (Parser::*)(), TABLE_SIZE> arr{};
            arr.fill(nullptr);

            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((arr[static_cast<size_t>(std::tuple_element_t<Is, Types>::type)] =
                  &Parser::instanceParsing<std::tuple_element_t<Is, Types>>), ...);
            }(std::make_index_sequence<N>{});

            return arr;
        }();

        auto idx = static_cast<size_t>(state);
        if (idx < TABLE_SIZE && table[idx]) {
            bool result = (this->*table[idx])();
            return result ? True : False;
        }
        m_state = Type::None;
        return False;
    }

public:
    explicit Parser(Receiver &receiver)
        : m_receiver(receiver)
    {
        std::apply([&](auto &...boundsArray) {
            ((boundsArray.fill({0, std::numeric_limits<qsizetype>::max()})), ...);
        }, m_bounds);
    }

    void addData(const QByteArray &additional)
    {
        m_array.append(additional);
        parse();
    }

    void parse()
    {
        while (true) {
            switch (m_state) {
            case Type::None: {
                if (m_array.size() < static_cast<qsizetype>(sizeof(quint16))) {
                    return;
                }

                auto type = read<Type, quint16>();
                if (!type.has_value()) [[unlikely]] {
                    m_array.remove(0, sizeof(quint16));
                    ++m_errorCount;
                    if (m_errorCount > ErrorLimit) {
                        m_errorCount = 0;
                        m_receiver.onPacketErrors();
                        return;
                    }
                    break;
                }

                m_state = *type;
                m_array.remove(0, sizeof(quint16));
                m_parsed = 0;
                [[fallthrough]];
            }
            default: {
                auto result = dispatch(m_state);
                if (result == False) {
                    return;
                }
                break;
            }
            }
        }
    }

    inline void clear()
    {
        m_array.clear();
        m_state = Type::None;
        m_parsed = 0;
        m_errorCount = 0;
    }

    // -------------------------------------------------------------------------
    // Set runtime bounds by providing the data member pointer
    // Example: parser.setBounds<&ServerImage::data>(0, 1024);
    // -------------------------------------------------------------------------
    template<auto DataPtr>
    void setBounds(qsizetype min, qsizetype max)
    {
        using Class = typename member_pointer_class<decltype(DataPtr)>::type;
        constexpr size_t packetIdx = type_index_in_tuple<Class, PacketTypes>::value;
        static_assert(packetIdx != static_cast<size_t>(-1), "The given member pointer does not belong to any known packet type.");

        using FieldTuple = decltype(Class::fields);
        constexpr size_t fieldIdx = find_sized_field_index<FieldTuple, decltype(DataPtr)>::value;
        static_assert(fieldIdx != static_cast<size_t>(-1),
                      "The provided member pointer does not correspond to a sized field's data member "
                      "(i.e., it is not the data part of a SizedField).");

        auto &boundsArray = std::get<packetIdx>(m_bounds);
        boundsArray[fieldIdx] = {min, max};
    }

    // Old index‑based version (optional)
    template<Type packetType, size_t FieldIndex>
    void setBounds(qsizetype min, qsizetype max)
    {
        using T = typename variant_types<PacketVariant>::template find<packetType>;
        constexpr size_t idx = type_index_in_tuple<T, PacketTypes>::value;
        auto &boundsArray = std::get<idx>(m_bounds);
        boundsArray[FieldIndex] = {min, max};
    }
};

// -----------------------------------------------------------------------------
// Writer
// -----------------------------------------------------------------------------

class Writer
{
    template<typename Class, typename SizeType, typename DataType, size_t Index,
             SizeType DefaultMin, SizeType DefaultMax>
    static void updateSizeIfNeeded(Class &obj,
                                   const SizedField<Class, SizeType, DataType, Index,
                                                    DefaultMin, DefaultMax> &field)
    {
        obj.*field.sizePtr = static_cast<SizeType>((obj.*field.dataPtr).size());
    }

    template<typename... Args>
    static void updateSizeIfNeeded(Args &&...)
    {}

    template<typename Class, typename T>
    static void writeField(QDataStream &stream, const Class &obj,
                           const FixedField<Class, T> &field)
    {
        stream << obj.*field.ptr;
    }

    template<typename Class, typename SizeType, typename DataType, size_t Index,
             SizeType DefaultMin, SizeType DefaultMax>
    static void writeField(QDataStream &stream, const Class &obj,
                           const SizedField<Class, SizeType, DataType, Index,
                                            DefaultMin, DefaultMax> &field)
    {
        stream << (obj.*field.dataPtr);
    }

public:
    template<typename T>
    static QByteArray generate(T &t)
    {
        constexpr auto fields = T::fields;

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((updateSizeIfNeeded(t, std::get<Is>(fields))), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});

        QByteArray output;
        QDataStream stream(&output, QIODeviceBase::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        stream << static_cast<quint16>(T::type);

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((writeField(stream, t, std::get<Is>(fields))), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});

        return output;
    }
};

} // namespace Packets

#endif // PARSER_H
