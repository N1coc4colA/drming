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
    ServerImage,
    ClientResolution,
    ServerBrightness,

    MINIMUM = ServerImage,
    MAXIMUM = ServerBrightness,

    LOWER = ServerImage,
    UPPER = MAXIMUM
};

/* Packets definitions */
struct ServerImage
{
    static constexpr Type type = Type::ServerImage;

    qsizetype imageSize;
    QByteArray data;

    using mQSizeType = qsizetype ServerImage::*;
    using mByteArray = QByteArray ServerImage::*;
    using mSized = std::pair<std::variant<mQSizeType>, mByteArray>;
    static constexpr std::array<std::variant<mQSizeType>, 1> members = {&ServerImage::imageSize};
    static constexpr std::array<mSized, 1> variableMembers = {mSized{&ServerImage::imageSize, &ServerImage::data}};
};

struct ClientResolution
{
    static constexpr Type type = Type::ClientResolution;

    quint32 width;
    quint32 height;

    using mQuint32 = quint32 ClientResolution::*;
    static constexpr std::array<std::variant<mQuint32>, 2> members = {&ClientResolution::width, &ClientResolution::height};
};

struct ServerBrightness
{
    static constexpr Type type = Type::ServerBrightness;

    qfloat16 brightness;

    using mQFloat16 = qfloat16 ServerBrightness::*;
    static constexpr std::array<std::variant<mQFloat16>, 1> members = {&ServerBrightness::brightness};
};

using PacketVariant = std::variant<ServerImage, ClientResolution, ServerBrightness>;

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
        if (o >= lower && o <= upper) {
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
using find_packet_t = typename find_packet<Wanted, Ts...>::type;

template<typename>
struct variant_types;

template<typename... Ts>
struct variant_types<std::variant<Ts...>>
{
    template<Type Wanted>
    using find = typename find_packet<Wanted, Ts...>::type;
};

template<typename V, Type Wanted>
using TypeToPacket = typename variant_types<V>::template find<Wanted>;

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

template<typename Receiver>
class Parser
{
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
    bool instanceParsing()
    {
        static constexpr size_t membersCount = T::members.size();

        if (!m_parsed) {
            m_current = T{};
        }

        // Parse fixed-size members
        while (m_parsed < membersCount) {
            const bool stalled = std::visit(
                [&](auto ptr) -> bool {
                    using MemberType = std::decay_t<decltype(std::declval<T>().*ptr)>;
                    if (m_array.size() < static_cast<qsizetype>(sizeof(MemberType))) {
                        return false;
                    }

                    const auto value = read<MemberType>();
                    if (!value.has_value()) {
                        // The boolean error indicates if the bytes should be skipped on failure.
                        if (value.error()) {
                            m_array.remove(0, sizeof(MemberType));
                        }

                        return true;
                    }

                    std::get<T>(m_current).*ptr = *value;
                    m_array.remove(0, sizeof(MemberType));
                    m_parsed++;

                    return false;
                },
                T::members[m_parsed]);

            if (stalled) {
                return false;
            }
        }

        // Parse variable-length members
        if constexpr (has_variableMembers<T>::value) {
            static constexpr size_t variableMembersCount = T::variableMembers.size();
            static constexpr size_t toProcess = membersCount + variableMembersCount;

            while (m_parsed < toProcess) {
                T &packet = std::get<T>(m_current);
                auto sized = T::variableMembers[m_parsed - membersCount];

                const auto len = std::visit([&](auto &&ptr) { return (packet.*ptr); }, sized.first);
                if (m_array.size() < static_cast<qsizetype>(len)) {
                    return false;
                }

                packet.*sized.second = m_array.first(static_cast<qsizetype>(len));
                m_array.remove(0, static_cast<qsizetype>(len));
                m_parsed++;
            }
        }

        m_receiver.processPacket(std::get<T>(m_current));

        m_state = Type::None;
        m_parsed = 0;
        return true;
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
            case Packets::Type::None: {
                if (m_array.size() < static_cast<qsizetype>(sizeof(quint16))) {
                    return;
                }

                const auto type = read<Type, quint16>();
                if (!type.has_value()) {
                    // Invalid type value — consume and try to resync
                    m_array.remove(0, sizeof(quint16));

                    if (!type.error()) {
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
                bool done = false;
                switch (m_state) {
                case Type::ServerImage: {
                    done = instanceParsing<ServerImage>();
                    break;
                }
                case Type::ClientResolution: {
                    done = instanceParsing<ClientResolution>();
                    break;
                }
                case Type::ServerBrightness: {
                    done = instanceParsing<ServerBrightness>();
                    break;
                }
                default: {
                    // Unknown state — reset and attempt resync
                    m_state = Type::None;
                    done = true;
                    break;
                }
                }

                if (!done) {
                    return;
                }
            }
            }
        }
    }

private:
    QByteArray m_array{};
    Packets::Type m_state = Packets::Type::None;
    PacketVariant m_current{};
    Receiver &m_receiver;
    size_t m_parsed = 0;
};

class Writer
{
public:
    template<typename T>
    static QByteArray generate(T &t)
    {
        static constexpr size_t variableMembersCount = has_variableMembers<T>::value ? (std::end(T::variableMembers) - std::begin(T::variableMembers))
                                                                                     : 0;
        static constexpr size_t membersCount = has_members<T>::value ? (std::end(T::members) - std::begin(T::members)) : 0;

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
};

} // namespace Packets

#endif // PARSER_H
