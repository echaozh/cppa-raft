#include <algorithm>

#include "raft.hpp"

using namespace std;
using namespace cppa;

partial_function who_am_i(address_type addr) {
    return on(atom("who")) >> [&]() {send(self->last_sender(), addr);};
}

partial_function handle_connections(peer_map& peers) {
    return (
        on(atom("address"), arg_match) >> [&](string host, uint16_t port) {
            auto& left = peers.left;
            auto peer = self->last_sender();
            auto it = left.find(make_pair(host, port));
            if(it != left.end()) {
                assert(it->second != peer);
                // duplicate connection, close what we initiate
                send(peer, atom("EXIT"), exit_reason::user_shutdown);
            } else {              // register and monitor the peer
                left.insert(peer_map::left_value_type(make_pair(host, port),
                                                      peer));
                self->monitor(peer);
            }
        },
        on(atom("DOWN"), arg_match) >> [&](uint32_t reason) {
            aout << "peer down due to " << exit_reason::as_string(reason)
                 << endl << flush;
            peers.right.erase(self->last_sender());
        });
}

optional<address_type> check_peer(const peer_map& peers) {
    auto peer = self->last_sender();
    const auto& right = peers.right;
    auto it = right.find(peer);
    if(it != right.end())
        return it->second;
    else {
        send(peer, atom("who"));
        return {};
    }
}
