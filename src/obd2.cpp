#include "../include/obd2.h"

#include <algorithm>
#include <future>
#include <stdexcept>

namespace obd2 {
    obd2::obd2() {}

    obd2::obd2(const char *if_name, uint32_t refresh_ms, bool enable_pid_chaining) 
        : protocol_instance(if_name, refresh_ms), enable_pid_chaining(enable_pid_chaining) {}

    obd2::obd2(obd2 &&o) {
        protocol_instance = std::move(o.protocol_instance);
        req_combinations = std::move(o.req_combinations);
        req_combinations_map = std::move(o.req_combinations_map);

        for (auto &p : req_combinations_map) {
            p.first->parent = this;
        }
    }

    obd2::~obd2() {
        for (auto &p : req_combinations_map) {
            p.first->parent = nullptr;
        }
    }

    obd2 &obd2::operator=(obd2 &&o) {
        if (this == &o) {
            return *this;
        }

        for (auto &p : req_combinations_map) {
            p.first->parent = nullptr;
        }

        protocol_instance = std::move(o.protocol_instance);
        req_combinations = std::move(o.req_combinations);
        req_combinations_map = std::move(o.req_combinations_map);

        for (auto &p : req_combinations_map) {
            p.first->parent = this;
        }

        return *this;
    }

    void obd2::set_refreshed_cb(const std::function<void(void)> &cb) {
        protocol_instance.set_refreshed_cb(cb);
    }

    void obd2::set_refresh_ms(uint32_t refresh_ms) {
        protocol_instance.set_refresh_ms(refresh_ms);
    }

    std::vector<uint8_t> obd2::get_supported_pids(uint32_t ecu_id) {
        std::vector<uint8_t> pids;

        for (uint8_t pid_range = 0; ; pid_range++) {            
            std::vector<uint8_t> pids_in_range = get_supported_pids(ecu_id, 0x01, pid_range * PID_SUPPORT_RANGE);
            pids.insert(pids.end(), pids_in_range.begin(), pids_in_range.end());
        
            // Check whether next supported pids command is supported
            if (pids_in_range.size() == 0) {
                break;
            }

            if (*(pids.end() - 1) != ((pid_range + 1) * PID_SUPPORT_RANGE)) {
                break;
            }
        }
        
        return pids;
    }
    
    std::vector<uint8_t> obd2::get_supported_pids(uint32_t ecu_id, uint8_t service, uint8_t pid_offset) {
        command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, service, pid_offset, protocol_instance);

        if (c.wait_for_response() != command::OK) {
            return std::vector<uint8_t>();
        }
        
        std::vector<uint8_t> data(c.get_buffer().begin() + 1, c.get_buffer().end());
        return decode_pids_supported(data, pid_offset);
    }

    std::vector<dtc> obd2::get_dtcs(uint32_t ecu_id) {
        std::vector<dtc> dtcs;
        dtc::status statuses[] = { dtc::STORED, dtc::PENDING, dtc::PERMANENT };

        for (dtc::status s : statuses) {
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, s, 0, protocol_instance);

            if (c.wait_for_response() != command::OK) {
                continue;
            }

            if (c.get_buffer().size() < 2) {
                continue;
            }

            std::vector<uint8_t> data(c.get_buffer().begin() + 1, c.get_buffer().end());
            std::vector<dtc> response_dtcs = decode_dtcs(data, s);
            dtcs.insert(dtcs.end(), response_dtcs.begin(), response_dtcs.end());
        }

        return dtcs;
    }

    vehicle_info obd2::get_vehicle_info() {
        vehicle_info info = { .vin = "Unkonwn", .ign_type = vehicle_info::UNKNOWN, .ecus = { } };
        std::vector<uint8_t> pids = get_supported_pids(ECU_ID_FIRST, 0x09, 0);

        info.ecus = get_ecus();
        
        // Try to get vin
        if (std::find(pids.begin(), pids.end(), 0x02) != pids.end()) {
            command c(ECU_ID_FIRST, ECU_ID_FIRST + ECU_ID_RES_OFFSET, 0x09, 0x02, protocol_instance);

            if (c.wait_for_response() == command::OK) {
                const std::vector<uint8_t> &res = c.get_buffer();
                info.vin = std::string(reinterpret_cast<const char *>(res.data() + 1));
            }
        }

        // Try to get ignition type
        info.ign_type = vehicle_info::UNKNOWN;

        if (std::find(pids.begin(), pids.end(), 0x08) != pids.end()) {
            info.ign_type = vehicle_info::SPARK;
        } 
        else if (std::find(pids.begin(), pids.end(), 0x0B) != pids.end()) {
            info.ign_type = vehicle_info::COMPRESSION;
        }

        return info;
    }

    void obd2::set_enable_pid_chaining(bool enable_pid_chaining) {
        this->enable_pid_chaining = enable_pid_chaining;
    }

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

        if (c.get_command().get_response_status() == command::ERROR) {
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
    
    std::vector<uint8_t> obd2::decode_pids_supported(const std::vector<uint8_t> &data, uint8_t pid_offset) {
        std::vector<uint8_t> pids;

        for (uint8_t byte : data) {
            for (int8_t bit = 7; bit >= 0; bit--) {
                pid_offset++;

                if (byte & (1 << bit)) {
                    pids.push_back(pid_offset);
                }
            }
        }

        return pids;
    }

    std::vector<dtc> obd2::decode_dtcs(const std::vector<uint8_t> &data, dtc::status status) {
        std::vector<dtc> dtcs;

        for (size_t i = 0; i < data.size(); i += 2) {
            uint16_t raw_code = data[i] | data[i + 1];

            dtcs.emplace_back(raw_code, status);
        }

        return dtcs;
    }

    std::vector<vehicle_info::ecu> obd2::get_ecus() {
        std::vector<std::future<bool>> ecu_futures;
        std::vector<vehicle_info::ecu> ecu_results;
        std::vector<vehicle_info::ecu> ecus;

        ecu_futures.reserve(ECU_ID_LAST - ECU_ID_FIRST + 1);
        ecu_results.reserve(ECU_ID_LAST - ECU_ID_FIRST + 1);

        // Asynchronously get information about each ECU
        for (uint32_t ecu_id = ECU_ID_FIRST; ecu_id <= ECU_ID_LAST; ecu_id++) {
            ecu_futures.emplace_back(
                std::async(
                    std::launch::async, 
                    &obd2::get_ecu, 
                    this, 
                    ecu_id, 
                    std::ref(ecu_results[ecu_id - ECU_ID_FIRST])
                )
            );
        }

        // Wait for all futures to finish
        for (size_t i = 0; i < ecu_futures.size(); i++) {
            if (!ecu_futures[i].get()) {
                continue;
            }

            ecus.emplace_back(std::move(ecu_results[i]));
        }

        return ecus;
    }

    bool obd2::get_ecu(uint32_t ecu_id, vehicle_info::ecu &ecu) {
        std::vector<uint8_t> pids = get_supported_pids(ecu_id, 0x09, 0);

        if (pids.size() == 0) {
            return false;
        }

        // Try to get ECU name
        if (std::find(pids.begin(), pids.end(), 0x0A) != pids.end()) {
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, 0x09, 0x0A, protocol_instance);

            if (c.wait_for_response() == command::OK) {
                const std::vector<uint8_t> &res = c.get_buffer();
                ecu.name.assign(reinterpret_cast<const char *>(res.data() + 1));
            }
        }

        ecu.id = ecu_id;

        return true;
    }
}
