#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <mutex>

namespace obd2 {
    class ecu {
        private:
            static const uint8_t RESPONSE_BUF_COUNT = 2;
            std::vector<uint8_t> response_bufs[RESPONSE_BUF_COUNT];
            bool response_updated = false;
            uint8_t cur_response_buf = 0;
            std::mutex response_bufs_mutex;

            uint32_t rx_id;
            
            void update_next_response_buf(const uint8_t *start, const uint8_t *end);

        public:
            ecu(uint32_t rx_id);
            ecu(ecu &e);
            ecu(ecu &&e);
            
            ecu &operator=(ecu &e);

            const std::vector<uint8_t> &get_current_response_buf();
            uint32_t get_rx_id();

            friend class instance;
    };
}
