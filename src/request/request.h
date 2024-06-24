#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace obd2 {
    class request {
        private:
            static const size_t RESPONSE_BUF_COUNT = 2;

            std::vector<uint8_t> response_bufs[RESPONSE_BUF_COUNT];
            size_t cur_response_buf = 0;
            bool response_updated = false;

            uint32_t tx_id;
            uint32_t rx_id = 0;
            uint8_t sid;
            uint16_t pid;
            bool refresh;

            std::vector<uint8_t> &get_next_response_buf();
            void update_response();
        
        public:
            request(uint32_t can_id, uint8_t sid, uint16_t pid, bool refresh);

            bool operator==(const request &r) const;
            
            const std::vector<uint8_t> &get_current_response_buf();

            friend class instance;
    };
}
