#ifndef CTRACK_WRAP_H
#define CTRACK_WRAP_H

#include <ctrack.hpp>

template<auto fn, typename Object, typename ... Args>
auto track(Object &obj, Args && ...args)
{
    CTRACK;

    return (obj->*fn)(args...);
}

#endif // CTRACK_WRAP_H
