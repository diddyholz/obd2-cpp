#pragma once

#include <atomic>
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
        
        public:
            command();
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, protocol &parent, bool refresh = false);
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids, protocol &parent, bool refresh = false);
            command(const command &c) = delete;
            command(command &&c);
            ~command();

            command &operator=(const command &c) = delete;
            command &operator=(command &&c);

            bool operator==(const command &c) const;

            /*
                When you are done with the command, call complete() to remove it from the protocol so that you can issue new commands with the same parameters.
                You will still be able to access the response buffer after calling complete().
                complete() is called automatically in the following cases:
                - The command is destroyed (e.g. goes out of scope)
                - The command is not set to be refreshed, and the response is received
            */
            void complete();
            void resume();
            void stop();
            uint32_t get_tx_id() const;
            uint32_t get_rx_id() const;
            uint8_t get_sid() const;
            const std::vector<uint16_t> &get_pids();
            void set_pids(const std::vector<uint16_t> &pids);
            void add_pid(uint16_t pid);
            void remove_pid(uint16_t pid);
            bool contains_pid(uint16_t pid);
            status get_response_status();
            status wait_for_response(uint32_t timeout_ms = 5000, uint32_t sample_us = 1000);
            const std::vector<uint8_t> &get_buffer();

        private:
            protocol *parent;

            std::vector<uint8_t> back_buffer;
            std::vector<uint8_t> response_buffer;
            std::mutex response_bufs_mutex;

            std::atomic<bool> response_updated = false;
            std::atomic<status> response_status = NO_RESPONSE;

            uint32_t tx_id;
            uint32_t rx_id;
            uint8_t sid;

            std::vector<uint16_t> pids;
            std::mutex pids_mutex;

            std::atomic<bool> refresh;

            void check_parent();
            std::vector<uint8_t> get_can_msg();
            void update_back_buffer(const uint8_t *start, const uint8_t *end);

            friend class protocol;
    };
}
