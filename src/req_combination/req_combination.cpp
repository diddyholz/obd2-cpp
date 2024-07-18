#include "req_combination.h"

#define OBD2_ID_OFFSET 0x08

namespace obd2 {
    req_combination::req_combination() { }

    req_combination::req_combination(uint32_t ecu_id, uint8_t sid, uint16_t pid, protocol &parent, bool refresh, bool allow_pid_chain)
        : cmd(ecu_id, ecu_id + OBD2_ID_OFFSET, sid, pid, parent, refresh), allow_pid_chain(allow_pid_chain) { }

    req_combination::req_combination(req_combination &&c) : cmd(std::move(c.cmd)), requests(std::move(c.requests)), allow_pid_chain(c.allow_pid_chain) { }
    
    req_combination &req_combination::operator=(req_combination &&c) {
        if (this == &c) {
            return *this;
        }

        cmd = std::move(c.cmd);
        allow_pid_chain = c.allow_pid_chain;
        requests = std::move(c.requests);

        return *this;
    }

    bool req_combination::operator==(const req_combination &c) const {
        return cmd == c.cmd;
    }

    void req_combination::add_request(request &r) {
        requests.push_back(std::ref(r));

        if (cmd.contains_pid(r.get_pid())) {
            return;
        }

        cmd.add_pid(r.get_pid());
    }

    bool req_combination::remove_request(request &r) {
        requests.erase(std::find(requests.begin(), requests.end(), r));

        for (request &req : requests) {
            if (req.get_pid() == r.get_pid()) {
                return false;
            }
        }

        cmd.remove_pid(r.get_pid());
        
        return requests.empty();
    }

    void req_combination::move_request(request &old_ref, request &new_ref) {
        requests.erase(std::find(requests.begin(), requests.end(), old_ref));
        requests.push_back(new_ref);
    }

    void req_combination::request_stopped() {
        // Check if command can be stopped
        for (request &r : requests) {
            if (r.get_refresh()) {
                return;
            }
        }

        cmd.stop();
    }

    void req_combination::request_resumed() {
        cmd.resume();
    }

    command &req_combination::get_command() {
        return cmd;
    }

    size_t req_combination::get_pid_count() {
        return cmd.get_pids().size();
    }

    bool req_combination::contains_pid(uint16_t pid) {
        return cmd.contains_pid(pid);
    }
    
    size_t req_combination::get_var_count(uint16_t pid) {
        size_t count = 0;

        for (request &r : requests) {
            if (r.get_pid() != pid) {
                continue;
            }
            
            size_t tmp = r.get_expected_size();

            if (tmp > count) {
                count = tmp;
            }
        }

        return count;
    }

    bool req_combination::get_allow_pid_chain() const {
        return allow_pid_chain;
    }
}
