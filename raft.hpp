/// /raft.hpp -- raft algorithm

/// Copyright (c) 2013 Vobile. All rights reserved.
/// Author: Zhang Yichao <zhang_yichao@vobile.cn>
/// Created: 2013-12-19
///

#ifndef INCLUDED_CPPA_RAFT_RAFT_HPP
#define INCLUDED_CPPA_RAFT_RAFT_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <boost/bimap.hpp>

#include <cppa/cppa.hpp>

typedef std::pair<std::string, uint16_t> address_type;

template <typename LogEntry>
struct append_request {
    uint64_t term;
    uint64_t prev_index;
    uint64_t prev_term;
    uint64_t committed;
    std::vector<LogEntry> entries;
};
template <typename LogEntry>
static inline bool operator==(const append_request<LogEntry>& lhs,
                              const append_request<LogEntry>& rhs) {
    return lhs.term == rhs.term && lhs.prev_index == rhs.prev_index
        && lhs.prev_term == rhs.prev_term && lhs.committed == rhs.committed
        && lhs.entries.size() == rhs.entries.size();
}

struct append_response {
    uint64_t term;
    bool succeeds;
};
static inline bool operator==(append_response lhs, append_response rhs) {
    return lhs.term == rhs.term && lhs.succeeds == rhs.succeeds;
}

struct vote_request {
    uint64_t term;
    uint64_t last_index;
    uint64_t last_term;
};
static inline bool operator==(vote_request lhs, vote_request rhs) {
    return lhs.term == rhs.term && lhs.last_index == rhs.last_index
        && lhs.last_term == rhs.last_term;
}

struct vote_response {
    uint64_t term;
    bool granted;
};
static inline bool operator==(vote_response lhs, vote_response rhs) {
    return lhs.term == rhs.term && lhs.granted == rhs.granted;
}

template <typename LogEntry>
void announce_protocol() {
    typedef append_request<LogEntry> appreq;
    cppa::announce<appreq>(&appreq::term, &appreq::prev_index,
                           &appreq::prev_term, &appreq::committed,
                           &appreq::entries);
    cppa::announce<append_response>(&append_response::term,
                              &append_response::succeeds);
    cppa::announce<vote_request>(&vote_request::term, &vote_request::last_index,
                                 &vote_request::last_term);
    cppa::announce<vote_response>(&vote_response::term, &vote_response::granted);
}

template <typename LogEntry>
struct working_config {
    address_type address;
    std::function<std::vector<LogEntry> (uint64_t first,
                                         uint64_t count)> read_logs;
    std::function<void (uint64_t prev_index, size_t from,
                        std::vector<LogEntry>)> write_logs;
};
typedef boost::bimap<address_type, cppa::actor_ptr> peer_map;
struct working_state {
    uint64_t term;
    uint64_t committed;
    uint64_t last_index;
    // cache the term for the last log, so when voting, we don't have to
    // read disks like we're crazy
    uint64_t last_term;
    peer_map peers;
};
struct follower_state {
    working_state working;
    cppa::optional<address_type> leader;
    cppa::optional<address_type> voted_for;
};

cppa::partial_function handle_connections(peer_map &peers);
cppa::partial_function who_am_i(address_type addr);
template <typename LogEntry>
cppa::partial_function follower(cppa::actor_ptr states,
                                const working_config<LogEntry> &config,
                                follower_state &state);

cppa::optional<address_type> check_peer(const peer_map &peers);

#endif // INCLUDED_CPPA_RAFT_RAFT_HPP
