/// /test_raft.hpp -- raft test fixture

/// Author: Zhang Yichao <echaozh#gmail.com>
/// Created: 2013-12-19
///

#ifndef INCLUDED_CPPA_RAFT_TEST_RAFT_HPP
#define INCLUDED_CPPA_RAFT_TEST_RAFT_HPP

#include <cstdint>

#include <gtest/gtest.h>

#include "candidate.hpp"
#include "follower.hpp"
#include "raft.hpp"

#include "cppa_test.hpp"
#include "prelude.hpp"

static inline bool operator==(const raft_state& lhs,
                              const raft_state& rhs) {
    return lhs.term == rhs.term && lhs.committed == rhs.committed
        && lhs.last_index == rhs.last_index && lhs.last_term == rhs.last_term
        && (lhs.leader == rhs.leader || (!lhs.leader && !rhs.leader))
        && (lhs.voted_for == rhs.voted_for || (!lhs.voted_for
                                               && !rhs.voted_for));
}
std::ostream& operator<<(std::ostream& s, const raft_state& st) {
    s << "working_state{" << "term = " << st.term
      << "; committed = " << st.committed << "; last_index = " << st.last_index
      << "; last_term = " << st.last_term << "; leader = {";
    if(st.leader)
        s << st.leader->first << ", " << st.leader->second;
    s << "}; voted_for = {";
    if(st.voted_for)
        s << st.voted_for->first << ", " << st.voted_for->second;
    return s << "}}";
}

std::ostream& operator<<(std::ostream& s, const append_response& resp) {
    return s << "append_response{" << "term = " << resp.term
             << "; succeeds = " << resp.succeeds << "}";
}
std::ostream& operator<<(std::ostream& s, const vote_response& resp) {
    return s << "vote_response{" << "term = " << resp.term
             << "; granted = " << resp.granted << "}";
}

struct test_log_entry {
    uint64_t term;
};
static inline bool operator==(test_log_entry lhs, test_log_entry rhs) {
    return lhs.term == rhs.term;
}

std::ostream& operator<<(std::ostream& s, const test_log_entry& log) {
    return s << "log_entry{" << "term = " << log.term << "}";
}

class RaftTest : public CppaTest {
protected:
    void ReportAddress() {
        cppa::send(raft_, cppa::atom("address"), addr_.first, addr_.second);
    }
    std::function<void ()> Quit(bool kill_states = false) {
        return [=]() {
            using namespace cppa;
            send(raft_, atom("EXIT"), exit_reason::user_shutdown);
            if(kill_states)
                send(states_, atom("EXIT"), exit_reason::user_shutdown);
            self->quit();
        };
    }
protected:
    cppa::actor_ptr raft_, states_;
    std::pair<std::string, uint16_t> addr_ = std::make_pair("localhost",
                                                            (uint16_t) 12346);
    raft_config<test_log_entry> config_;
    raft_state state_, backup_state_;
};

#endif // INCLUDED_CPPA_TEST_TEST_RAFT_HPP
