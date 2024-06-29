#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace obd2 {
    class ecu {
        private:
            static const uint8_t RESPONSE_BUF_COUNT = 2;
            std::vector<uint8_t> response_bufs[RESPONSE_BUF_COUNT];
            uint8_t cur_response_buf = 0;

            uint32_t rx_id;
            bool response_updated;
            
            void update_response();
            std::vector<uint8_t> &get_next_response_buf();

        public:
            ecu(uint32_t rx_id);

            const std::vector<uint8_t> &get_current_response_buf();
            uint32_t get_rx_id();

            friend class instance;
    };
}
