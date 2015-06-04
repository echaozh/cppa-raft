/// /cppa_test.hpp -- cppa based app test fixture base

/// Author: Zhang Yichao <echaozh@gmail.com>
/// Created: 2013-12-19
///

#ifndef INCLUDED_CPPA_RAFT_CPPA_TEST_HPP
#define INCLUDED_CPPA_RAFT_CPPA_TEST_HPP

#include <cppa/cppa.hpp>

class CppaTest : public testing::Test {
protected:
    virtual void TearDown() {
        cppa::await_all_others_done();
        cppa::shutdown();
    }
};

#endif // INCLUDED_CPPA_RAFT_CPPA_TEST_HPP
