#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../src/req_combination/req_combination.h"
#include "../src/protocol/protocol.h"
#include "../src/request/request.h"
#include "../src/dtc/dtc.h"
#include "../src/vehicle_info/vehicle_info.h"
#include "../src/ecu/ecu.h"

namespace obd2 {
    class obd2 {
        public:
            obd2();
            obd2(const char *if_name, uint32_t refresh_ms = 1000, bool enable_pid_chaining = false);
            obd2(const obd2 &i) = delete;
            obd2(obd2 &&i);
            ~obd2();

            obd2 &operator=(const obd2 &i) = delete;
            obd2 &operator=(obd2 &&i);

            bool is_connection_active();
            void set_enable_pid_chaining(bool enable_pid_chaining);
            void set_refreshed_cb(const std::function<void(void)> &cb);
            void set_refresh_ms(uint32_t refresh_ms);
            std::vector<std::reference_wrapper<ecu>> get_ecus();
            const vehicle_info &get_vehicle_info();
            std::vector<uint8_t> get_supported_pids(uint32_t ecu_id, uint8_t service);
            bool pid_supported(uint32_t ecu_id, uint8_t service, uint16_t pid);
            std::vector<dtc> get_dtcs(uint32_t ecu_id);
            void clear_dtcs(uint32_t ecu_id);
            
        private:
            // TODO: Enums for service and pids
            static constexpr uint32_t ECU_ID_BROADCAST  = 0x7DF;
            static constexpr uint32_t ECU_ID_FIRST      = 0x7E0;
            static constexpr uint32_t ECU_ID_LAST       = 0x7E7;
            static constexpr uint32_t ECU_ID_RES_OFFSET = 0x08;

            static constexpr uint8_t PID_SUPPORT_RANGE  = 0x20;

            protocol protocol_instance;
            bool enable_pid_chaining = false;

            std::list<req_combination> req_combinations;
            std::unordered_map<request *, std::reference_wrapper<req_combination>> req_combinations_map;

            std::unordered_map<uint32_t, ecu> ecus; // ECU ID => ECU
            vehicle_info vehicle;

            std::mutex connection_mutex;
            std::atomic<bool> last_connection_active = false;
            std::atomic<uint8_t> connection_request_id = 0; // Used to identify connection requests results

            bool query_connection_status();
            void query_standard_ecus();
            ecu query_ecu(uint32_t ecu_id, uint8_t query_service = 0x09);
            void query_vehicle_info();
            std::vector<uint8_t> get_supported_pids(uint32_t ecu_id, uint8_t service, bool cache);
            std::vector<uint8_t> get_supported_pids(uint32_t ecu_id, uint8_t service, uint8_t pid_offset);
            std::vector<uint8_t> decode_pids_supported(const std::vector<uint8_t> &data, uint8_t pid_offset);
            std::vector<dtc> decode_dtcs(const std::vector<uint8_t> &data, dtc::status status);

            void add_request(request &r);
            void remove_request(request &r);
            void move_request(request &old_ref, request &new_ref);
            void stop_request(request &r);            
            void resume_request(request &r);
            std::vector<uint8_t> get_data(request &r);

            req_combination &get_combination(uint32_t ecu_id, uint8_t service, uint16_t pid, bool allow_pid_chain);     

            friend class request;
    };
}
