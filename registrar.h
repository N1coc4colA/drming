#ifndef REGISTRAR_H
#define REGISTRAR_H

#include <utility>
#include <vector>

template<typename T>
class Registrar
{
public:
    static Registrar &instance()
    {
        static Registrar s_instance;
        return s_instance;
    }

    void add(T entry)
    {
        m_entries.emplace_back(std::move(entry));
    }

    const std::vector<T> &entries() const
    {
        return m_entries;
    }

    size_t count() const
    {
        return m_entries.size();
    }

    struct AutoRegister
    {
        template<typename... Args>
        explicit AutoRegister(Args &&...args)
        {
            Registrar<T>::instance().add(T(std::forward<Args>(args)...));
        }
    };

private:
    Registrar() = default;
    std::vector<T> m_entries{};
};

#endif // REGISTRAR_H
