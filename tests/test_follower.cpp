#include <chrono>
#include <vector>

#include "follower.hpp"

#include "test_raft.hpp"

using namespace std;
using namespace std::chrono;
using namespace cppa;

struct test_log_entry {
    uint64_t term;
};
static inline bool operator==(test_log_entry lhs, test_log_entry rhs) {
    return lhs.term == rhs.term;
}

bool operator==(const follower_state& lhs, const follower_state& rhs) {
    return lhs.working == rhs.working
        && (lhs.leader == rhs.leader || (!lhs.leader && !rhs.leader))
        && (lhs.voted_for == rhs.voted_for || (!lhs.voted_for
                                               && !rhs.voted_for));
}
ostream& operator<<(ostream& s, const follower_state& st) {
    s << "follower_state{" << "leader = {";
    if(st.leader)
        s << st.leader->first << ", " << st.leader->second;
    s << "}; voted_for = {";
    if(st.voted_for)
        s << st.voted_for->first << ", " << st.voted_for->second;
    return s << "}; working = " << st.working << "}";
}
ostream& operator<<(ostream& s, const test_log_entry& log) {
    return s << "log_entry{" << "term = " << log.term << "}";
}

class FollowerTest : public RaftTest {
protected:
    typedef append_request<test_log_entry> appreq;
    virtual void SetUp() {
        announce<test_log_entry>(&test_log_entry::term);
        announce_protocol<test_log_entry>();
        config_ = {
            // address
            make_pair("localhost", (uint16_t) 12345),
            // read_logs()
            [=](uint64_t first, uint64_t count) -> vector<test_log_entry> {
                if(logs_.size() < first)
                    return vector<test_log_entry>();
                else {
                    auto it = (logs_.size() < first + count ? end(logs_)
                               : begin(logs_) + first + count);
                    return vector<test_log_entry>(begin(logs_) + first, it);
                }
            },
            // write_logs()
            [=](uint64_t prev_index, size_t from,
                const vector<test_log_entry> &logs) {
                logs_.resize(prev_index + 1 + logs.size());
                copy(begin(logs) + from, end(logs),
                     begin(logs_) + prev_index + 1 + from);
            }
        };
        logs_ = {{0}, {1}, {2}, {2}, {3}, {3}, {3}};
        state_ = {{
                100,                // term
                0,                  // committed
                6,                  // last_index
                3,                  // last_term
            }};
        states_ = spawn([=]() {
                become(
                    on(atom("expect"), arg_match) >> [=](uint64_t to) {
                        Become(
                            [=]() {self->quit();},
                            on(atom("apply_to"), to) >> [=]() {self->quit();});
                    });
            });
        raft_ = spawn([=]() {become(follower(states_, config_, state_));});
    }
    template<typename... Ts>
    void Become(function<void ()> f, Ts&&... args) {
        become(
            forward<Ts>(args)...,
            others() >> [=]() {
                ADD_FAILURE() << "Unrecognized message: "
                              << to_string(self->last_dequeued());
                f();
            },
            after(seconds(1)) >> [=]() {
                ADD_FAILURE() << "No message received";
                f();
            });
    }
    template <typename Request, typename Response>
    void TestActor(Request &&req, Response resp, bool is_leader,
                   optional<uint64_t> committed,
                   function<void ()> update_backup) {
        spawn([=]() {
                backup_logs_ = logs_;
                backup_state_ = state_;

                self->monitor(raft_);
                self->monitor(states_);
                auto ta = spawn([=]() {
                        ReportAddress();
                        send(raft_, move(req));
                        Become(
                            [=]() {
                                send(states_, atom("EXIT"),
                                     exit_reason::user_shutdown);
                                Quit();
                            },
                            on_arg_match >> [=](Response real) {
                                EXPECT_EQ(resp, real) << "Incorrect response";
                                update_backup();
                                if(is_leader)
                                    backup_state_.leader = addr_;
                                if(committed)
                                    send(states_, atom("expect"), *committed);
                                else {
                                    send(states_, atom("EXIT"),
                                         exit_reason::user_shutdown);
                                }
                                EXPECT_EQ(backup_logs_, logs_)
                                    << "Incorrect logs";
                                EXPECT_EQ(backup_state_, state_)
                                    << "Incorrect state";
                                Quit();
                            });
                    });
                self->monitor(ta);
                become(
                    on(atom("DOWN"), arg_match) >> [=](uint32_t) {
                        if(++deaths_ == 3)
                            self->quit();
                    });
            });
    }
    working_config<test_log_entry> config_;
    follower_state state_, backup_state_;
    vector<test_log_entry> logs_, backup_logs_;
    size_t deaths_ = 0;
};

// append when leader has lesser term
TEST_F(FollowerTest, AppendLesserTerm) {
    TestActor(appreq{1}, append_response{100, false}, false, {}, []() {});
}

// append when follower doesn't have matching previous log
TEST_F(FollowerTest, AppendNoPrevLog) {
    TestActor(
        appreq{
            1000,       // term
                1000,   // prev_index
                1000,   // prev_term
                },
        append_response{1000, false},
        true, {},
        [=]() {
            send(states_, atom("EXIT"), exit_reason::user_shutdown);
            backup_state_.working.term = 1000;
        });
};

// append when follower must discard logs
TEST_F(FollowerTest, AppendDiscardLogs) {
    TestActor(
        appreq{
            100,          // term
                0,        // perv_index
                0,        // prev term
                2,        // committed
                {{1}, {2}, {3}}, // entries
                },
        append_response{100, true},
        true, 2,
        [=]() {
            backup_logs_.resize(4);
            backup_logs_[3] = {3};
            backup_state_.working.committed = 2;
            backup_state_.working.last_index = 3;
            backup_state_.working.last_term = 3;
        });
}

// append when leader is really fast with very late commits
TEST_F(FollowerTest, AppendFastLeader) {
    TestActor(
        appreq{
            1000,          // term
                6,        // perv_index
                3,        // prev term
                100,        // committed
                {{4}, {5}, {6}}, // entries
                },
        append_response{1000, true},
        true, 9,
        [=]() {
            backup_logs_.push_back({4});
            backup_logs_.push_back({5});
            backup_logs_.push_back({6});
            backup_state_.working.term = 1000;
            backup_state_.working.committed = 9; // last index is 9
            backup_state_.working.last_index = 9;
            backup_state_.working.last_term = 6;
        });
};

// vote when candidate term is lesser
TEST_F(FollowerTest, VoteLesserTerm) {
    TestActor(vote_request{10},            // term
              vote_response{100, false},
              false, {}, []() {});
}

// vote when candidate is not who we voted for
TEST_F(FollowerTest, VoteAfterAnother) {
    state_.voted_for = {make_pair("localhost", (uint16_t) 54321)};
    TestActor(vote_request{1000}, // term
              vote_response{1000, false},
              false, {}, [=]() {backup_state_.working.term = 1000;});
}

// vote when candidate is not up to date
TEST_F(FollowerTest, VoteForSnail) {
    state_.voted_for = addr_;
    TestActor(vote_request{
            100,               // term
                10,             // last_index
                1,              // last_term
                },
        vote_response{100, false},
        false, {}, []() {});
}

// vote again for the same candidate
TEST_F(FollowerTest, VoteAgain) {
    state_.voted_for = addr_;
    TestActor(vote_request{
            1000,               // term
                10,             // last_index
                10,             // last_term
                },
        vote_response{1000, true},
        false, {}, [=]() {backup_state_.working.term = 1000;});
}

// vote and forget about leader
TEST_F(FollowerTest, VoteForgetLeader) {
    state_.leader = addr_;
    TestActor(vote_request{
            1000,               // term
                10,             // last_index
                10,             // last_term
                },
        vote_response{1000, true},
        false, {}, [=]() {
            backup_state_.leader = {};
            backup_state_.voted_for = addr_;
            backup_state_.working.term = 1000;
        });
}

// test follower reaction without address reporting first
// TEST_F(FollowerTest, NoAddress) {
//     spawn([=]() {
//             send(raft_, atom("test"));
//             become(
//                 on(atom("who")) >> [=]() {
//                     aout << "who received" << endl << flush;
//                     Quit();
//                 },
//                 others() >> [=]() {
//                     EXPECT_TRUE(false) << "unexpected response received: "
//                                        << to_string(self->last_dequeued());
//                     Quit();
//                 },
//                 after(seconds(1)) >> [=]() {
//                     EXPECT_TRUE(false) << "no response received";
//                     Quit();
//                 });
//         });
// }
