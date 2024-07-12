#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <list>
#include <mutex>

namespace obd2 {
    class protocol;
    
    class command {
        private:
            protocol *parent;

            static const size_t RESPONSE_BUF_COUNT = 2;
            std::vector<uint8_t> response_bufs[RESPONSE_BUF_COUNT];
            bool response_updated = false;
            size_t cur_response_buf = 0;
            std::mutex response_bufs_mutex;

            uint32_t tx_id;
            uint32_t rx_id;
            uint8_t sid;
            uint16_t pid;
            bool refresh;

            std::vector<uint8_t> get_can_msg() const;
            void update_next_buf(const uint8_t *start, const uint8_t *end);
        
        public:
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, bool refresh, protocol &parent);
            command(command &e);
            command(command &&e);

            command &operator=(command &e);
            bool operator==(const command &r) const;

            void resume();            
            void stop();
            uint32_t get_tx_id() const;
            uint32_t get_rx_id() const;
            uint8_t get_sid() const;
            uint16_t get_pid() const;

            const std::vector<uint8_t> &get_current_buf();

            friend class protocol;
    };
}
