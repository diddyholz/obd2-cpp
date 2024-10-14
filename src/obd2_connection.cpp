#include "../include/obd2.h"

#include <future>

namespace obd2 {
    bool obd2::is_connection_active() {
        // Check if main ecu is responding
        command c(ECU_ID_FIRST, ECU_ID_FIRST + ECU_ID_RES_OFFSET, 0x01, 0x00, protocol_instance);

        // If not, return false and clear connection data
        if (c.wait_for_response() != command::OK) {
            ecus.clear();
            vehicle = vehicle_info();
            return false;
        }

        // If yes, query standard ECUs and vehicle info if not already done
        if (ecus.size() == 0) {
            query_standard_ecus();
            query_vehicle_info();
        }

        return true;
    }

    const vehicle_info &obd2::get_vehicle_info() {
        is_connection_active();
        
        return vehicle;
    }
    
    std::vector<std::reference_wrapper<ecu>> obd2::get_ecus() {
        is_connection_active();

        std::vector<std::reference_wrapper<ecu>> ecu_list;
        ecu_list.reserve(ecus.size());

        for (auto &pair : ecus) {
            ecu_list.emplace_back(pair.second);
        }

        return ecu_list;
    }

    void obd2::query_standard_ecus() {
        std::vector<std::future<ecu>> ecu_futures;

        ecu_futures.reserve(ECU_ID_LAST - ECU_ID_FIRST + 1);

        // Asynchronously get information about each ECU
        for (uint32_t ecu_id = ECU_ID_FIRST; ecu_id <= ECU_ID_LAST; ecu_id++) {
            ecu_futures.emplace_back(
                std::async(
                    std::launch::async, 
                    &obd2::query_ecu, 
                    this, 
                    ecu_id,
                    0x09
                )
            );
        }

        // Wait and process results
        for (size_t i = 0; i < ecu_futures.size(); i++) {
            ecu result = ecu_futures[i].get();
            
            if (result.get_id() == 0) {
                continue;
            }

            ecus[result.get_id()] = result;
        }
    }

    ecu obd2::query_ecu(uint32_t ecu_id, uint8_t query_service) {
        std::vector<uint8_t> pids = get_supported_pids(ecu_id, query_service, false);
        std::vector<uint8_t> pids_bak;
        std::string ecu_name;
        ecu result;

        if (pids.size() == 0) {
            return result;
        }

        // Fetch service 0x09 pids if the service used to query the ECU is not 0x09
        if (query_service != 0x09) {
            pids_bak = pids;
            pids = get_supported_pids(ecu_id, 0x09, false);
        }

        // Try to get ECU name
        if (std::find(pids.begin(), pids.end(), 0x0A) != pids.end()) {
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, 0x09, 0x0A, protocol_instance);

            if (c.wait_for_response() == command::OK) {
                const std::vector<uint8_t> &res = c.get_buffer();
                ecu_name.assign(reinterpret_cast<const char *>(res.data() + 1));
            }
        }

        result = ecu(ecu_id, ecu_name);

        if (query_service != 0x01) {
            result.add_supported_pids(0x01, get_supported_pids(ecu_id, 0x01, false));
        }

        if (query_service != 0x02) {
            result.add_supported_pids(0x02, get_supported_pids(ecu_id, 0x02, false));
        }

        result.add_supported_pids(0x09, pids);

        if (query_service != 0x09) {
            result.add_supported_pids(query_service, pids_bak);
        }

        return result;
    }

    void obd2::query_vehicle_info() {
        vehicle = { .vin = "Unkonwn", .ign_type = vehicle_info::UNKNOWN };
        std::vector<uint8_t> pids = get_supported_pids(ECU_ID_FIRST, 0x09);

        // Try to get vin
        if (std::find(pids.begin(), pids.end(), 0x02) != pids.end()) {
            command c(ECU_ID_FIRST, ECU_ID_FIRST + ECU_ID_RES_OFFSET, 0x09, 0x02, protocol_instance);

            if (c.wait_for_response() == command::OK) {
                std::vector<uint8_t> res = c.get_buffer();
                res.push_back(0);
                
                vehicle.vin = std::string(reinterpret_cast<const char *>(res.data() + 1));
            }
        }

        // Try to get ignition type
        if (std::find(pids.begin(), pids.end(), 0x08) != pids.end()) {
            vehicle.ign_type = vehicle_info::SPARK;
        } 
        else if (std::find(pids.begin(), pids.end(), 0x0B) != pids.end()) {
            vehicle.ign_type = vehicle_info::COMPRESSION;
        }
    }

    std::vector<uint8_t> obd2::get_supported_pids(uint32_t ecu_id, uint8_t service) {
        return get_supported_pids(ecu_id, service, true);
    }

    bool obd2::pid_supported(uint32_t ecu_id, uint8_t service, uint16_t pid) {
        std::vector<uint8_t> pids = get_supported_pids(ecu_id, service);

        return std::find(pids.begin(), pids.end(), pid) != pids.end();
    }

    std::vector<uint8_t> obd2::get_supported_pids(uint32_t ecu_id, uint8_t service, bool cache) {
        std::vector<uint8_t> pids;

        // If no standard service, return empty pids
        if (service != 0x01 && service != 0x02 && service != 0x09) {
            return pids;
        }

        // Check if requested pids are already cached
        if (cache) {
            auto it = ecus.find(ecu_id);

            // If not even the ecu is cached, query it first
            if (it == ecus.end()) {
                ecu e = query_ecu(ecu_id, service);

                // If ECU has no connection return empty pids
                if (e.get_id() == 0) {
                    return pids;
                }

                ecus[ecu_id] = e;

                it = ecus.find(ecu_id);
            }

            pids = it->second.get_supported_pids(service);

            if (pids.size() > 0) {
                return pids;
            }
        }

        // If no pids are cached, query them
        for (uint8_t pid_range = 0; ; pid_range++) {            
            std::vector<uint8_t> pids_in_range = get_supported_pids(ecu_id, service, uint8_t(pid_range * PID_SUPPORT_RANGE));
            pids.insert(pids.end(), pids_in_range.begin(), pids_in_range.end());
        
            // Check whether next supported pids command is supported
            if (pids_in_range.size() == 0) {
                break;
            }

            if (*(pids.end() - 1) != ((pid_range + 1) * PID_SUPPORT_RANGE)) {
                break;
            }
        }

        if (cache) {
            ecus[ecu_id].add_supported_pids(service, pids);
        }
        
        return pids;
    }
    
    std::vector<uint8_t> obd2::get_supported_pids(uint32_t ecu_id, uint8_t service, uint8_t pid_offset) {
        command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, service, pid_offset, protocol_instance);

        if (c.wait_for_response() != command::OK) {
            return std::vector<uint8_t>();
        }

        const std::vector<uint8_t> &response = c.get_buffer();
        std::vector<uint8_t> data(response.begin() + 1, response.end());
        return decode_pids_supported(data, pid_offset);
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
}
