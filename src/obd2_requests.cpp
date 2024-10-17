#include "../include/obd2.h"

namespace obd2 {
    void obd2::add_request(request &r) {
        // Check if request already exists
        for (auto &p : req_combinations_map) {
            request *existing_r = p.first;
            
            if (existing_r->ecu_id == r.ecu_id 
                && existing_r->service == r.service 
                && existing_r->pid == r.pid 
                && existing_r->formula_str == r.formula_str) {
                throw std::invalid_argument("A request with the specified parameters already exists");
            }
        }

        req_combination &c = get_combination(r.ecu_id, r.service, r.pid, r.refresh && !r.formula_str.empty() && enable_pid_chaining);
        c.add_request(r);

        req_combinations_map.emplace(&r, c);
    }

    void obd2::remove_request(request &r) {
        stop_request(r);

        req_combination &c = req_combinations_map.at(&r);
        req_combinations_map.erase(&r);

        if (c.remove_request(r)) {
            req_combinations.remove(c);
        }

        r.parent = nullptr;
    }

    void obd2::move_request(request &old_ref, request &new_ref) {
        req_combination &c = req_combinations_map.at(&old_ref);
        c.move_request(old_ref, new_ref);

        req_combinations_map.erase(&old_ref);
        req_combinations_map.emplace(&new_ref, c);
    }

    req_combination &obd2::get_combination(uint32_t ecu_id, uint8_t service, uint16_t pid, bool allow_pid_chain) {
        // First, check if any command already contains the requested pid
        for (auto &p : req_combinations_map) {
            command &c = p.second.get().get_command();

            if (c.get_tx_id() == ecu_id && c.get_sid() == service && c.contains_pid(pid)) {
                return p.second;
            }
        }       

        // If not, check if there is any command to which the pid request can be added to
        if (allow_pid_chain && (service == 0x01 || service == 0x02)) {
            for (auto &p : req_combinations_map) {
                req_combination &c = p.second.get();
                command &cmd = c.get_command();

                if (cmd.get_tx_id() != ecu_id || cmd.get_sid() != service) {
                    continue;
                }
                
                if (!c.get_allow_pid_chain()) {
                    continue;
                }

                if (c.get_pid_count() >= 6 && !c.contains_pid(pid)) {
                    continue;
                }

                return c;
            }
        }

        return req_combinations.emplace_back(
            ecu_id, service, pid, protocol_instance, true, allow_pid_chain && (service == 0x01 || service == 0x02)
        );
    }

    void obd2::resume_request(request &r) {
        if (r.refresh) {
            return;
        }

        r.refresh = true;

        req_combination &c = req_combinations_map.at(&r);
        c.request_resumed();
    }

    void obd2::stop_request(request &r) {
        if (!r.refresh) {
            return;
        }
    
        r.refresh = false;

        req_combination &c = req_combinations_map.at(&r);
        c.request_stopped();
    }

    std::vector<uint8_t> obd2::get_data(request &r) {
        req_combination &c = req_combinations_map.at(&r);
        const std::vector<uint8_t> &data = c.get_command().get_buffer();
        std::vector<uint8_t> decoded_data;

        if (c.get_command().get_response_status() == cmd_status::ERROR) {
            return decoded_data;
        }

        if (data.size() == 0) {
            return data;
        }

        // If the request is not part of a chain, return the raw response minus the pid at the front
        if (c.get_pid_count() == 1) {
            decoded_data = std::vector<uint8_t>(data.begin() + 1, data.end());
            return decoded_data;
        }

        // Else decode the response
        decoded_data.reserve(r.get_expected_size());

        for (size_t i = 0; i < data.size(); ) {
            if (data[i] != r.pid) {
                i += c.get_var_count(data[i]) + 1;
                continue;
            }

            auto data_start = data.begin() + i + 1;

            decoded_data = std::vector<uint8_t>(data_start, data_start + r.get_expected_size());
            break;
        }

        return decoded_data;
    }
}
