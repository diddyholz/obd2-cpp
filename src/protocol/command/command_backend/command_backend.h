#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <list>
#include <mutex>

#include "cmd_status.h"

namespace obd2 {
    class protocol;
    
    class command_backend {        
        public:
            command_backend();
            command_backend(uint32_t tx_id, uint32_t rx_id, uint8_t sid, protocol &parent, bool refresh = false);
            command_backend(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, protocol &parent, bool refresh = false);
            command_backend(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids, protocol &parent, bool refresh = false);
            command_backend(const command_backend &c) = delete;
            command_backend(command_backend &&c);
            ~command_backend();

            command_backend &operator=(const command_backend &c) = delete;
            command_backend &operator=(command_backend &&c);

            bool operator==(const command_backend &c) const;

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
            cmd_status get_response_status();
            cmd_status wait_for_response(uint32_t timeout_ms = 5000, uint32_t sample_us = 1000);
            const std::vector<uint8_t> &get_buffer();

        private:
            protocol *parent;

            std::vector<uint8_t> back_buffer;
            std::vector<uint8_t> response_buffer;
            std::mutex response_bufs_mutex;

            std::atomic<bool> response_updated = false;
            std::atomic<cmd_status> response_status = WAITING;

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
            friend class command;
    };
}
