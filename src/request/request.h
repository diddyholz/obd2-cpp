#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <list>
#include "ecu/ecu.h"

namespace obd2 {
    class request {
        private:
            std::list<ecu> ecus;

            uint32_t tx_id;
            uint8_t sid;
            uint16_t pid;
            bool refresh;

            void get_can_msg(std::vector<uint8_t> &buf);
            ecu &get_ecu_by_id(uint32_t rx_id);
        
        public:
            request(uint32_t can_id, uint8_t sid, uint16_t pid, bool refresh);

            bool operator==(const request &r) const;
            
            uint32_t get_tx_id() const;
            uint8_t get_sid() const;
            uint16_t get_pid() const;
            std::list<ecu> &get_ecus();

            friend class instance;
    };
}
