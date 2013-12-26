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
    return lhs.working == rhs.working && lhs.leader == rhs.leader;
}
ostream& operator<<(ostream& s, const follower_state& st) {
    return s << "follower_state{" << "leader = {" << st.leader.first
             << ", " << st.leader.second
             << "}; working = " << st.working << "}";
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
        backup_logs_ = logs_ = {{0}, {1}, {2}, {2}, {3}, {3}, {3}};
        backup_state_ = state_ = {100};
        states_ = spawn([=]() {
                become(
                    on(atom("expect"), arg_match) >> [=](uint64_t to) {
                        Become(
                            [=]() {self->quit();},
                            on(atom("apply_to"), to) >> [=]() {self->quit();});
                    });
            });
        raft_ = spawn([=]() {
                become(
                    handle_connections(state_.working.peers),
                    follower(states_, config_, state_));
            });
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
    void TestActor(Request &&req, Response resp,
                   function<bool ()> update_states) {
        spawn([=]() {
                self->monitor(raft_);
                self->monitor(states_);
                auto ta = spawn([=]() {
                        ReportAddress();
                        send(raft_, move(req));
                        Become(
                            [=]() {Quit();},
                            on_arg_match >> [=](Response real) {
                                EXPECT_EQ(resp, real) << "Incorrect response";
                                if(update_states())
                                    backup_state_.leader = addr_;
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
    TestActor(appreq{1}, append_response{100, false},
              [=]() -> bool {
                  send(states_, atom("EXIT"), exit_reason::user_shutdown);
                  return false;
              });
};

// append when follower doesn't have matching previous log
TEST_F(FollowerTest, AppendNoPrevLog) {
    TestActor(
        appreq{
            1000,       // term
                1000,   // prev_index
                1000,   // prev_term
                },
        append_response{1000, false},
        [=]() -> bool {
            send(states_, atom("EXIT"), exit_reason::user_shutdown);
            backup_state_.working.term = 1000;
            return true;
        });
};

// append when follower must discard logs
TEST_F(FollowerTest, AppendDiscardLogs) {
    send(states_, atom("expect"), (uint64_t) 2);
    TestActor(
        appreq{
            100,          // term
                0,        // perv_index
                0,        // prev term
                2,        // committed
                {{1}, {2}, {3}}, // entries
                },
        append_response{100, true},
        [=]() -> bool {
            backup_logs_.resize(4);
            backup_logs_[3] = {3};
            backup_state_.working.committed = 2;
            return true;
        });
};

// append when leader is really fast with very late commits
TEST_F(FollowerTest, AppendFastLeader) {
    send(states_, atom("expect"), (uint64_t) 9);
    TestActor(
        appreq{
            1000,          // term
                6,        // perv_index
                3,        // prev term
                100,        // committed
                {{4}, {5}, {6}}, // entries
                },
        append_response{1000, true},
        [=]() -> bool {
            backup_logs_.push_back({4});
            backup_logs_.push_back({5});
            backup_logs_.push_back({6});
            backup_state_.working.term = 1000;
            backup_state_.working.committed = 9; // last index is 9
            return true;
        });
};

// // vote when leader term is lesser
// TEST_F(FollowerTest, VoteLesserTerm) {
//     spawn([=]() {
//             self->monitor(raft_);
//             self->monitor(states_);
//             auto test = TestActor(vote_request{10},            // term
//                                   vote_response{100, false},
//                                   [=]() -> bool {return false;});
//             self->monitor(test);
//             aout << "follower actor: " << raft_ << endl << "state machine: "
//                  << states_ << endl << "test actor: " << test << endl << flush;
//             become(
//                 on(atom("DOWN"), arg_match) >> [=](uint32_t reason) {
//                     aout << "dead actor: " << self->last_sender() << endl
//                          << flush;
//                 });
//         });
// }

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
