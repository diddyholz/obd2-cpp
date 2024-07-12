#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../src/protocol/protocol.h"
#include "../src/request/request.h"

namespace obd2 {

    class obd2 {
        private:
            protocol protocol_instance;

            std::unordered_map<request *, std::reference_wrapper<command>> requests_command_map;

            size_t requests_using_command(const command &c) const;
            void stop_request(request &r);            
            void resume_request(request &r);     
            const std::vector<uint8_t> &get_data(const request &r);     

        public:
            obd2(const char *if_name, uint32_t refresh_ms = 1000);
            obd2(const obd2 &i) = delete;
            obd2(const obd2 &&i) = delete;
            ~obd2();

            obd2 &operator=(const obd2 &i) = delete;

            void set_refresh_ms(uint32_t refresh_ms);
            request &add_request(uint32_t ecu_id, uint8_t service, uint16_t pid, const std::string &formula, bool refresh);

            friend class request;
    };
}
