/// /follower.hpp -- follower template implementation

/// Copyright (c) 2013 Vobile. All rights reserved.
/// Author: Zhang Yichao <zhang_yichao@vobile.cn>
/// Created: 2013-12-19
///

#ifndef INCLUDED_CPPA_RAFT_FOLLOWER_HPP
#define INCLUDED_CPPA_RAFT_FOLLOWER_HPP

#include "raft.hpp"

template <typename LogEntry>
size_t check_logs(const working_config<LogEntry> &config, uint64_t prev_index,
                  const std::vector<LogEntry> &entries) {
    auto count = entries.size();
    auto logs = config.read_logs(prev_index + 1, count);
    auto count2 = logs.size();
    assert(count2 <= count);
    for(size_t i = 0; i < count2; ++i) {
        if(logs[i].term != entries[i].term)
            return i;
    }
    return count2;
}

template <typename LogEntry>
static cppa::partial_function
follower_append(cppa::actor_ptr states, const working_config<LogEntry> &config,
                follower_state &state) {
    using namespace std;
    using namespace cppa;
    return (
        on_arg_match >> [&, states](const append_request<LogEntry> &req) {
            auto &working = state.working;
            auto leader = check_peer(working.peers);
            if(!leader)
                return;
            bool succeeds = false;
            if(req.term >= working.term) {
                // update leader address
                state.leader = *leader;
                if(req.term > working.term)
                    working.term = req.term;
                auto logs = config.read_logs(req.prev_index, 1);
                if(!logs.empty() && logs[0].term == req.prev_term) {
                    succeeds = true;
                    auto from = check_logs(config, req.prev_index, req.entries);
                    config.write_logs(req.prev_index, from, req.entries);
                    auto last_index = req.prev_index + req.entries.size();
                    if(req.committed > working.committed)
                        working.committed = min(req.committed, last_index);
                    // make the state machine actor apply up to the latest log
                    send(states, atom("apply_to"), working.committed);
                }
            }
            send(self->last_sender(), append_response {working.term, succeeds});
        });
}

template <typename LogEntry>
cppa::partial_function follower(cppa::actor_ptr states,
                                const working_config<LogEntry> &config,
                                follower_state &state) {
    return (
        follower_append(states, config, state)
        );
}

#endif // INCLUDED_CPPA_RAFT_FOLLOWER_HPP
