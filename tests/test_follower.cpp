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
        states_ = spawn([]() {become(others() >> []() {});});
        raft_ = spawn([=]() {
                become(
                    handle_connections(state_.working.peers),
                    follower(states_, config_, state_));
            });
    }
    function<void ()> AppendActor(appreq &&req, append_response resp,
                                  function<bool ()> update_states) {
        return [=]() {
            ReportAddress();
            send(raft_, move(req));
            become(
                on_arg_match >> [=](append_response real) {
                    EXPECT_EQ(resp, real) << "Incorrect append response";
                    if(update_states())
                        backup_state_.leader = addr_;
                    EXPECT_EQ(backup_logs_, logs_)
                        << "Incorrect logs after append";
                    EXPECT_EQ(backup_state_, state_)
                        << "Incorrect state after append";
                    Quit();
                },
                others() >> [=]() {
                    ADD_FAILURE() << "Unrecognized response: "
                                  << to_string(self->last_dequeued());
                    Quit();
                },
                after(seconds(1)) >> [=]() {
                    ADD_FAILURE() << "No response received";
                    Quit();
                });
        };
    }
    working_config<test_log_entry> config_;
    follower_state state_, backup_state_;
    vector<test_log_entry> logs_, backup_logs_;
};

// append when leader has lesser term
TEST_F(FollowerTest, AppendLesserTerm) {
    spawn(AppendActor(appreq{1}, append_response{100, false},
                      []() -> bool {return false;}));
};

// test append when follower doesn't have matching previous log
TEST_F(FollowerTest, AppendNoPrevLog) {
    spawn(AppendActor(
              appreq{
                  1000,       // term
                      1000,   // prev_index
                      1000,   // prev_term
                      },
              append_response{1000, false},
              [=]() -> bool {
                  backup_state_.working.term = 1000;
                  return true;
              }));
};

// test append when follower must discard logs
TEST_F(FollowerTest, AppendDiscardLogs) {
    spawn(AppendActor(
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
              }));
};

// test append when follower must discard logs
TEST_F(FollowerTest, AppendFastLeader) {
    spawn(AppendActor(
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
              }));
};

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
