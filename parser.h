#ifndef PARSER_H
#define PARSER_H

#include <QDataStream>
#include <QDebug>

#include <QFloat16>
#include <qtypes.h>

#include <expected>
#include <type_traits>

namespace Packets {

/* Types of packet */
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

/* Packets definitions */
struct HeartBeat
{
    static constexpr auto type = Type::HeartBeat;
};

struct Reinit
{
    static constexpr auto type = Type::Reinit;
};

struct ServerImage
{
    static constexpr auto type = Type::ServerImage;

    qsizetype formatSize = 0;
    qsizetype imageSize = 0;
    QByteArray format;
    QByteArray data;

    using mQSizeType = qsizetype ServerImage::*;
    using mByteArray = QByteArray ServerImage::*;
    using mSized = std::pair<std::variant<mQSizeType>, mByteArray>;
    static constexpr std::array<std::variant<mQSizeType>, 2> members = {&ServerImage::formatSize, &ServerImage::imageSize};
    static constexpr std::array<mSized, 2> variableMembers = {mSized{&ServerImage::formatSize, &ServerImage::format},
                                                              mSized{&ServerImage::imageSize, &ServerImage::data}};
};

struct ClientResolution
{
    static constexpr auto type = Type::ClientResolution;

    quint32 width = 0;
    quint32 height = 0;

    using mQuint32 = quint32 ClientResolution::*;
    static constexpr std::array<std::variant<mQuint32>, 2> members = {&ClientResolution::width, &ClientResolution::height};
};

struct ServerBrightness
{
    static constexpr auto type = Type::ServerBrightness;

    qfloat16 brightness;

    using mQFloat16 = qfloat16 ServerBrightness::*;
    static constexpr std::array<std::variant<mQFloat16>, 1> members = {&ServerBrightness::brightness};
};

struct ServerStream
{
    static constexpr auto type = Type::ServerStream;

    qsizetype frameSize = 0;
    QByteArray data;

    using mQSizeType = qsizetype ServerStream::*;
    using mByteArray = QByteArray ServerStream::*;
    using mSized = std::pair<std::variant<mQSizeType>, mByteArray>;
    static constexpr std::array<std::variant<mQSizeType>, 1> members = {&ServerStream::frameSize};
    static constexpr std::array<mSized, 1> variableMembers = {mSized{&ServerStream::frameSize, &ServerStream::data}};
};

using PacketVariant = std::variant<HeartBeat, Reinit, ServerImage, ClientResolution, ServerBrightness, ServerStream>;

/* Typing & indexing reflection */

template<typename T, typename... Ts>
struct PacketIndex_counter;

template<typename T, typename First, typename... Rest>
struct PacketIndex_counter<T, First, Rest...>
{
private:
    static constexpr std::size_t next = PacketIndex_counter<T, Rest...>::value;

public:
    static constexpr std::size_t value = std::is_same_v<T, First> ? 0 : (next == static_cast<std::size_t>(-1) ? next : next + 1);
};

template<typename T>
struct PacketIndex_counter<T>
{
    static constexpr std::size_t value = static_cast<std::size_t>(-1); // not found
};

template<std::size_t I, typename... Ts>
struct type_at;

template<typename T, typename... Ts>
struct type_at<0, T, Ts...>
{
    using type = T;
};

template<std::size_t I, typename T, typename... Ts>
struct type_at<I, T, Ts...>
{
    static_assert(I < sizeof...(Ts) + 1, "index out of range");
    using type = typename type_at<I - 1, Ts...>::type;
};

template<std::size_t I, typename... Ts>
using type_at_t = typename type_at<I, Ts...>::type;

/* Types validators */
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
    // From Type to O
    template<typename O>
    static constexpr std::expected<O, bool> validateTo(const Type &t)
    {
        return static_cast<O>(t);
    }

    // From O to Type — no convertibility constraint since scoped enums aren't implicitly convertible
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

/* Inverse mapping from Type enum value -> packet struct */

template<typename T>
concept PacketLike = requires {
    { T::type } -> std::convertible_to<Type>;
};

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

template<typename V, Type Wanted>
using TypeToPacket = variant_types<V>::template find<Wanted>;

/* Packet parser */
template<typename, typename = void>
struct has_members : std::false_type
{};

template<typename T>
struct has_members<T, std::void_t<decltype(T::members)>> : std::true_type
{};

template<typename, typename = void>
struct has_variableMembers : std::false_type
{};

template<typename T>
struct has_variableMembers<T, std::void_t<decltype(T::variableMembers)>> : std::true_type
{};

template<typename, typename = void>
struct count_members
{
    static constexpr size_t value = 0;
};

template<typename T>
    requires(has_members<T>::value)
struct count_members<T>
{
    static constexpr size_t value = T::members.size();
};

template<typename, typename = void>
struct count_variableMembers
{
    static constexpr size_t value = 0;
};

template<typename T>
    requires(has_members<T>::value)
struct count_variableMembers<T>
{
    static constexpr size_t value = T::variableMembers.size();
};

template<typename Receiver, const int ErrorLimit = 5>
class Parser
{
    enum ParseOutput {
        False = 0,
        True = 1,
        Continue,
    };

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

    template<typename T>
    ParseOutput instanceMembersParsing()
    {
        static constexpr size_t membersCount = T::members.size();

        // Parse fixed-size members
        while (m_parsed < membersCount) {
            const bool stalled = std::visit(
                [&](auto ptr) -> bool {
                    using MemberType = std::decay_t<decltype(std::declval<T>().*ptr)>;
                    if (m_array.size() < static_cast<qsizetype>(sizeof(MemberType))) {
                        return True;
                    }

                    const auto value = read<MemberType>();
                    if (!value.has_value()) {
                        // The boolean error indicates if the bytes should be skipped on failure.
                        if (value.error()) {
                            m_array.remove(0, sizeof(MemberType));
                            m_errorCount++;
                            if (m_errorCount > ErrorLimit) {
                                m_errorCount = 0;
                                m_receiver.onPacketErrors();
                            }
                        }

                        return True;
                    }

                    std::get<T>(m_current).*ptr = *value;
                    m_array.remove(0, sizeof(MemberType));
                    m_parsed++;

                    return False;
                },
                T::members[m_parsed]);

            if (stalled) {
                return False;
            }
        }

        return Continue;
    }

    template<typename T>
    ParseOutput instanceVariableMembersParsing()
    {
        static constexpr size_t variableMembersCount = T::variableMembers.size();
        static constexpr size_t membersCount = count_members<T>::value;
        static constexpr size_t toProcess = membersCount + variableMembersCount;

        while (m_parsed < toProcess) {
            T &packet = std::get<T>(m_current);
            auto sized = T::variableMembers[m_parsed - membersCount];

            using sizingType = qsizetype;

            const auto len = std::visit([&](auto &&ptr) { return packet.*ptr; }, sized.first);

            // Also consider invalid signed data, giving a negative value instead of positive one.
            if (len < 0) {
                if (!m_array.isEmpty()) {
                    m_array.remove(0, 1);
                }

                m_state = Type::None;
                m_parsed = 0;

                return True;
            }

            if (m_array.size() < static_cast<sizingType>(len)) {
                return False;
            }

            packet.*sized.second = m_array.first(static_cast<sizingType>(len));
            m_array.remove(0, static_cast<sizingType>(len));
            m_parsed++;
        }

        return Continue;
    }

    template<typename T>
    bool instanceParsing()
    {
        if (!m_parsed) {
            m_current = T{};
        }

        if constexpr (has_members<T>::value) {
            switch (instanceMembersParsing<T>()) {
            case False:
                return false;
            case True:
                return true;
            default:
                break;
            }
        }

        // Parse variable-length members
        if constexpr (has_variableMembers<T>::value) {
            switch (instanceVariableMembersParsing<T>()) {
            case False:
                return false;
            case True:
                return true;
            default:
                break;
            }
        }

        m_receiver.processPacket(std::get<T>(m_current));

        m_state = Type::None;
        m_parsed = 0;
        m_errorCount = 0;
        return true;
    }

    template<typename Variant>
    ParseOutput dispatch(Type state)
    {
        using Types = typename variant_types<Variant>::types;
        constexpr size_t N = std::tuple_size_v<Types>;

        // Table size: MAXIMUM value + 1 (includes index 0 for None)
        constexpr size_t TABLE_SIZE = static_cast<size_t>(Type::MAXIMUM) + 1;

        // Build a table of **member function pointers**
        constexpr auto table = [] {
            std::array<bool (Parser::*)(), TABLE_SIZE> arr{};
            arr.fill(nullptr);

            // Fold over the tuple types – use Parser::instanceParsing<...>
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((arr[static_cast<size_t>(std::tuple_element_t<Is, Types>::type)] = &Parser::instanceParsing<std::tuple_element_t<Is, Types>>), ...);
            }(std::make_index_sequence<N>{});

            return arr;
        }();

        auto idx = static_cast<size_t>(state);
        if (idx < TABLE_SIZE && table[idx]) {
            bool result = (this->*table[idx])();
            return result ? True : False;
        }

        return Continue;
    }

public:
    explicit Parser(Receiver &receiver)
        : m_receiver(receiver)
    {}

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

                const auto type = read<Type, quint16>();
                if (!type.has_value()) [[unlikely]] {
                    // Invalid type value – consume 2 bytes and count as error
                    m_array.remove(0, sizeof(quint16));
                    m_errorCount++;
                    if (m_errorCount > ErrorLimit) {
                        m_errorCount = 0;
                        m_receiver.onPacketErrors();
                        return; // onPacketErrors() likely clears the buffer, so stop parsing
                    }
                    break; // continue the while loop to resync on next bytes
                }

                m_state = *type;
                m_array.remove(0, sizeof(quint16));
                m_parsed = 0;
                [[fallthrough]];
            }
            default: {
                bool done = false;
                const auto parseOutput = dispatch<PacketVariant>(m_state);
                switch (parseOutput) {
                case Continue: {
                    // Unknown state — reset and attempt resync
                    m_state = Type::None;
                    done = true;
                    break;
                }
                default: {
                    done = static_cast<bool>(parseOutput);
                }
                }

                if (!done) {
                    return;
                }
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

private:
    QByteArray m_array{};
    Type m_state = Type::None;
    PacketVariant m_current{};
    Receiver &m_receiver;
    size_t m_parsed = 0;
    int m_errorCount = 0;
};

class Writer
{
public:
    template<typename T>
        requires(has_variableMembers<T>::value and has_members<T>::value)
    static QByteArray generate(T &t)
    {
        static constexpr size_t variableMembersCount = has_variableMembers<T>::value ? std::end(T::variableMembers) - std::begin(T::variableMembers)
                                                                                     : 0;
        static constexpr size_t membersCount = has_members<T>::value ? std::end(T::members) - std::begin(T::members) : 0;

        // Update size fields from actual data lengths before serializing
        for (size_t i = 0; i < variableMembersCount; i++) {
            auto sized = T::variableMembers[i];
            std::visit([&](auto ptr) { t.*ptr = static_cast<std::decay_t<decltype(t.*ptr)>>((t.*sized.second).size()); }, sized.first);
        }

        QByteArray output{};
        QDataStream stream(&output, QIODeviceBase::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        // Write packet type header
        stream << static_cast<quint16>(T::type);

        // Write fixed-size members
        for (size_t i = 0; i < membersCount; i++) {
            std::visit([&](auto ptr) { stream << t.*ptr; }, T::members[i]);
        }

        // Write variable-length data
        for (size_t i = 0; i < variableMembersCount; i++) {
            output += t.*T::variableMembers[i].second;
        }

        return output;
    }

    template<typename T>
        requires(has_variableMembers<T>::value and !has_members<T>::value)
    static QByteArray generate(T &t)
    {
        static constexpr size_t variableMembersCount = has_variableMembers<T>::value ? std::end(T::variableMembers) - std::begin(T::variableMembers)
                                                                                     : 0;

        // Update size fields from actual data lengths before serializing
        for (size_t i = 0; i < variableMembersCount; i++) {
            auto sized = T::variableMembers[i];
            std::visit([&](auto ptr) { t.*ptr = static_cast<std::decay_t<decltype(t.*ptr)>>((t.*sized.second).size()); }, sized.first);
        }

        QByteArray output{};
        QDataStream stream(&output, QIODeviceBase::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        // Write packet type header
        stream << static_cast<quint16>(T::type);

        // Write variable-length data
        for (size_t i = 0; i < variableMembersCount; i++) {
            output += t.*T::variableMembers[i].second;
        }

        return output;
    }

    template<typename T>
        requires(!has_variableMembers<T>::value and has_members<T>::value)
    static QByteArray generate(const T &t)
    {
        static constexpr size_t membersCount = has_members<T>::value ? std::end(T::members) - std::begin(T::members) : 0;

        QByteArray output{};
        QDataStream stream(&output, QIODeviceBase::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        // Write packet type header
        stream << static_cast<quint16>(T::type);

        // Write fixed-size members
        for (size_t i = 0; i < membersCount; i++) {
            std::visit([&](auto ptr) { stream << t.*ptr; }, T::members[i]);
        }

        return output;
    }

    template<typename T>
        requires(!has_variableMembers<T>::value and !has_members<T>::value)
    static QByteArray generate(const T &t)
    {
        QByteArray output{};
        QDataStream stream(&output, QIODeviceBase::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        // Write packet type header
        stream << static_cast<quint16>(T::type);

        return output;
    }
};

} // namespace Packets

#endif // PARSER_H
