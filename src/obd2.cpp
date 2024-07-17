#include "../include/obd2.h"

#include <algorithm>
#include <future>
#include <stdexcept>

namespace obd2 {
    obd2::obd2(const char *if_name, uint32_t refresh_ms) 
        : protocol_instance(if_name, refresh_ms) {}

    obd2::~obd2() {
        for (auto &p : requests_command_map) {
            p.first->parent = nullptr;
        }
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
            if (pids[pids.size() - 1] != (pid_range + 1) * PID_SUPPORT_RANGE) {
                break;
            }
        }
        
        return pids;
    }
    
    std::vector<uint8_t> obd2::get_supported_pids(uint32_t ecu_id, uint8_t service, uint8_t pid_offset) {
        command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, service, pid_offset, false, protocol_instance);

        if (c.wait_for_response() != command::OK) {
            return std::vector<uint8_t>();
        }

        return decode_pids_supported(c.get_buffer(), pid_offset);
    }

    std::vector<dtc> obd2::get_dtcs(uint32_t ecu_id) {
        std::vector<dtc> dtcs;
        dtc::status statuses[] = { dtc::STORED, dtc::PENDING, dtc::PERMANENT };

        for (dtc::status s : statuses) {
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, s, 0, false, protocol_instance);

            // Wait 60 seconds for response
            if (c.wait_for_response(60 * 1000) != command::OK) {
                continue;
            }

            if (c.get_buffer().size() == 0) {
                continue;
            }

            std::vector<dtc> response_dtcs = decode_dtcs(c.get_buffer(), s);
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
            command c(ECU_ID_FIRST, ECU_ID_FIRST + ECU_ID_RES_OFFSET, 0x09, 0x02, false, protocol_instance);

            if (c.wait_for_response() == command::OK) {
                info.vin = std::string(c.get_buffer().begin(), c.get_buffer().end());
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

    void obd2::add_request(request &r) {
        // Check if request already exists
        for (auto &p : requests_command_map) {
            request *existing_r = p.first;
            
            if (existing_r->ecu_id == r.ecu_id 
                && existing_r->service == r.service 
                && existing_r->pid == r.pid 
                && existing_r->formula_str == r.formula_str) {
                throw std::invalid_argument("A request with the specified parameters already exists");
            }
        }

        command &c = get_command(r.ecu_id, r.service, r.pid);
        requests_command_map.emplace(&r, c);
    }

    void obd2::remove_request(request &r) {
        stop_request(r);

        command &c = requests_command_map.at(&r);
        requests_command_map.erase(&r);

        // Check if command is still used by other requests, if not remove it
        for (auto p : requests_command_map) {
            if (&p.second.get() == &c) {
                return;
            }
        }

        commands.remove(c);
    }

    void obd2::move_request(request &old_ref, request &new_ref) {
        command &c = requests_command_map.at(&old_ref);
        requests_command_map.erase(&old_ref);
        requests_command_map.emplace(&new_ref, c);
    }

    command &obd2::get_command(uint32_t ecu_id, uint8_t service, uint16_t pid) {
        for (auto &c : commands) {
            if (c.get_tx_id() == ecu_id && c.get_sid() == service && c.get_pid() == pid) {
                return c;
            }
        }       
        
        // If no command was found, create a new one
        return commands.emplace_back(ecu_id, ecu_id + ECU_ID_RES_OFFSET, service, pid, true, protocol_instance);
    }
    
    // TODO: For faster counting, use a separate map<command *, size_t> instead of iterating over all requests
    size_t obd2::requests_using_command(const command &c) const {
        size_t count = 0;

        for (auto &p : requests_command_map) {
            if (&p.second.get() != &c) {
                continue;
            }

            if (p.first->refresh || !p.first->has_value()) {
                count++;
            }
        }

        return count;
    }

    void obd2::stop_request(request &r) {
        if (!r.refresh) {
            return;
        }
    
        r.refresh = false;

        command &c = requests_command_map.at(&r);

        if (requests_using_command(c) == 0) {
            c.stop();
        }
    }

    void obd2::resume_request(request &r) {
        if (r.refresh) {
            return;
        }

        r.refresh = true;
        requests_command_map.at(&r).get().resume();
    }

    const std::vector<uint8_t> &obd2::get_data(const request &r) {
        command &c = requests_command_map.at(const_cast<request *>((&r)));

        if (c.get_response_status() == command::ERROR) {
            static const std::vector<uint8_t> error_data = { };
            return error_data;
        }

        return c.get_buffer();
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
            uint16_t code = (data[i] << 8) | data[i + 1];

            dtc error_code = { 
                .cat = dtc::category(data[i] >> 6),
                .code = code,
                .stat = status
            };

            dtcs.push_back(error_code);
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
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, 0x09, 0x0A, false, protocol_instance);

            if (c.wait_for_response() == command::OK) {
                const std::vector<uint8_t> &res = c.get_buffer();
                ecu.name.assign(reinterpret_cast<const char *>(res.data()));
            }
        }

        ecu.id = ecu_id;

        return true;
    }
}
