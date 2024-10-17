#pragma once

#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <vector>
#include <unordered_map>

#include "command_backend/cmd_status.h"

namespace obd2 {
    class command_backend;
    class protocol;

    class command {
        public:
            command();
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, protocol &parent, bool refresh = false);
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, protocol &parent, bool refresh = false);
            command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids, protocol &parent, bool refresh = false);
            ~command();

            bool operator==(const command &c) const;

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
            cmd_status get_response_status();
            cmd_status wait_for_response(uint32_t timeout_ms = 5000, uint32_t sample_us = 1000);
            const std::vector<uint8_t> &get_buffer();

        private:
            command_backend *active_backend;

            command_backend &find_backend(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids,
                protocol &parent, bool refresh);
            void remove_backend(command_backend &c);

            // The following three resources could have been beautiful statics, but because of CPPs unpredicatbly static destruction order, 
            // they are not. This is a workaround to avoid memory leaks. Without this workaround the statics would be destroyed before the 
            // command instances, which would cause undefined behaviours.
            std::list<command_backend> &get_command_backends();
            std::unordered_map<command_backend *, std::atomic<uint32_t>> &get_command_usage();
            std::mutex &get_command_mutex();
    };
}
