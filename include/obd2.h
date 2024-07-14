#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../src/protocol/protocol.h"
#include "../src/request/request.h"
#include "../src/dtc/dtc.h"
#include "../src/vehicle_info/vehicle_info.h"

namespace obd2 {

    class obd2 {
        // TODO: Enums for service and pids
        private:
            static constexpr uint32_t ECU_ID_BROADCAST  = 0x7DF;
            static constexpr uint32_t ECU_ID_FIRST      = 0x7E0;
            static constexpr uint32_t ECU_ID_LAST       = 0x7E7;
            static constexpr uint32_t ECU_ID_RES_OFFSET = 0x08;

            static constexpr uint8_t PID_SUPPORT_RANGE  = 0x20;

            protocol protocol_instance;

            // TODO: Thread safety
            std::list<command> commands;
            std::unordered_map<request *, std::reference_wrapper<command>> requests_command_map;

            void add_request(request &r);
            void remove_request(request &r);
            void move_request(request &old_ref, request &new_ref);
            void stop_request(request &r);            
            void resume_request(request &r);     
            const std::vector<uint8_t> &get_data(const request &r);

            command &get_command(uint32_t ecu_id, uint8_t service, uint16_t pid);     
            size_t requests_using_command(const command &c) const;
            std::vector<uint8_t> decode_pids_supported(const std::vector<uint8_t> &data, uint8_t pid_offset);
            std::vector<dtc> decode_dtcs(const std::vector<uint8_t> &data, dtc::status status);
            std::vector<uint8_t> get_supported_pids(uint32_t ecu_id, uint8_t service, uint8_t pid_offset);
            std::vector<vehicle_info::ecu> get_ecus();
            bool get_ecu(uint32_t ecu_id, vehicle_info::ecu &ecu);

        public:
            obd2(const char *if_name, uint32_t refresh_ms = 1000);
            obd2(const obd2 &i) = delete;
            obd2(const obd2 &&i) = delete;
            ~obd2();

            obd2 &operator=(const obd2 &i) = delete;

            void set_refresh_ms(uint32_t refresh_ms);
            std::vector<uint8_t> get_supported_pids(uint32_t ecu_id);
            std::vector<dtc> get_dtcs(uint32_t ecu_id);
            vehicle_info get_vehicle_info();

            friend class request;
    };
}
