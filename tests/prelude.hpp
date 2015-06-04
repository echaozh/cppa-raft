/// /prelude.hpp -- functional prelude

/// Author: Zhang Yichao <echaozh@gmail.com>
/// Created: 2013-12-27
///

#ifndef INCLUDED_CPPA_RAFT_PRELUDE_HPP
#define INCLUDED_CPPA_RAFT_PRELUDE_HPP

#include <functional>

template<typename T>
T concat(const T& a, const T& b) {
    T c = a;
    c.insert(end(c), begin(b), end(b));
    return c;
}

template<typename T>
std::function<T ()> constant(const T& v) {
    return [=]() -> T {return v;};
}

#endif // INCLUDED_CPPA_RAFT_PRELUDE_HPP
