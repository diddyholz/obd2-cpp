#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <list>
#include <mutex>

namespace obd2 {
    class protocol;
    
    class command {
        public:
            enum status {
                OK = 0,
                NO_RESPONSE = 1,
                ERROR = 2
            };

        private:
            protocol *parent;

            std::mutex response_bufs_mutex;
            std::vector<uint8_t> back_buffer;
            std::vector<uint8_t> response_buffer;
            bool response_updated = false;
            status response_status = NO_RESPONSE;

            uint32_t tx_id;
            uint32_t rx_id;
            uint8_t sid;
            uint16_t pid;
            bool refresh;

            void check_parent();
            std::vector<uint8_t> get_can_msg();
            void update_back_buffer(const uint8_t *start, const uint8_t *end);
        
        public:
            command();
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, bool refresh, protocol &parent);
            command(const command &c) = delete;
            command(command &&c);
            ~command();

            command &operator=(const command &c) = delete;
            command &operator=(command &&c);

            bool operator==(const command &c) const;

            void resume();            
            void stop();
            uint32_t get_tx_id() const;
            uint32_t get_rx_id() const;
            uint8_t get_sid() const;
            uint16_t get_pid() const;

            status get_response_status();
            status wait_for_response(uint32_t timeout_ms = 10000, uint32_t sample_us = 1000);
            const std::vector<uint8_t> &get_buffer();

            friend class protocol;
    };
}
