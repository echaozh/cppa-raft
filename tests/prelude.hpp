/// /prelude.hpp -- functional prelude

/// Copyright (c) 2013 Vobile. All rights reserved.
/// Author: Zhang Yichao <zhang_yichao@vobile.cn>
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
