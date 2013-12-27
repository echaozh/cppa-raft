/// /test_raft.hpp -- raft test fixture

/// Copyright (c) 2013 Vobile. All rights reserved.
/// Author: Zhang Yichao <zhang_yichao@vobile.cn>
/// Created: 2013-12-19
///

#ifndef INCLUDED_CPPA_RAFT_TEST_RAFT_HPP
#define INCLUDED_CPPA_RAFT_TEST_RAFT_HPP

#include <cstdint>

#include <gtest/gtest.h>

#include "raft.hpp"

#include "cppa_test.hpp"

static inline bool operator==(const working_state& lhs,
                              const working_state& rhs) {
    return lhs.term == rhs.term && lhs.committed == rhs.committed
        && lhs.last_index == rhs.last_index && lhs.last_term == rhs.last_term;
}
std::ostream& operator<<(std::ostream& s, const working_state& st) {
    return s << "working_state{" << "term = " << st.term
             << "; committed = " << st.committed
             << "; last_index = " << st.last_index
             << "; last_term = " << st.last_term << "}";
}
std::ostream& operator<<(std::ostream& s, const append_response& resp) {
    return s << "append_response{" << "term = " << resp.term
             << "; succeeds = " << resp.succeeds << "}";
}
std::ostream& operator<<(std::ostream& s, const vote_response& resp) {
    return s << "vote_response{" << "term = " << resp.term
             << "; vote_granted = " << resp.vote_granted << "}";
}

class RaftTest : public CppaTest {
protected:
    void ReportAddress() {
        cppa::send(raft_, cppa::atom("address"), addr_.first, addr_.second);
    }
    void Quit() {
        using namespace cppa;
        send(raft_, atom("EXIT"), exit_reason::user_shutdown);
        self->quit();
    }
protected:
    cppa::actor_ptr raft_, states_;
    std::pair<std::string, uint16_t> addr_ = std::make_pair("localhost",
                                                            (uint16_t) 12346);
};

#endif // INCLUDED_CPPA_TEST_TEST_RAFT_HPP
