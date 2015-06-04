/// /candidate.hpp -- candidate behavior implementation

/// Author: Zhang Yichao <echaozh@gmail.com>
/// Created: 2013-12-27
///

#ifndef INCLUDED_CPPA_RAFT_CANDIDATE_HPP
#define INCLUDED_CPPA_RAFT_CANDIDATE_HPP

#include "raft.hpp"

template <typename LogEntry>
cppa::behavior candidate(cppa::actor_ptr states,
                         const raft_config<LogEntry>& config,
                         raft_state& state) {
    // FIXME: dummy impl
    using namespace cppa;
    return on_arg_match >> [](int) {};
}

#endif // INCLUDED_CPPA_RAFT_CANDIDATE_HPP
