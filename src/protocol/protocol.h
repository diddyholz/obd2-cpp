#pragma once

#include <atomic>
#include <cstdint>
#include <list>
#include <functional>
#include <vector>
#include <thread>
#include <unordered_map>
#include <mutex>

#include "command/command.h"
#include "socket_wrapper/socket_wrapper.h"

namespace obd2 {
    class protocol {
        private:
            // TODO: Use mapping from socket to command for faster lookup
            std::unordered_map<command *, std::reference_wrapper<socket_wrapper>> command_socket_map;
            std::mutex commands_mutex;

            std::vector<socket_wrapper> sockets;
            std::mutex sockets_mutex;

            unsigned int if_index;
            std::atomic<uint32_t> refresh_ms;
            std::thread listener_thread;
            std::atomic<bool> listener_running = false;

            std::function<void(void)> refreshed_cb;
            std::mutex refreshed_cb_mutex;

            void command_listener();
            void process_commands();
            void process_sockets();
            void process_command(command &c);
            void process_socket(socket_wrapper &s);
            socket_wrapper &get_socket(uint32_t tx_id, uint32_t rx_id);
            void add_command(command &c);
            void remove_command(command &c);
            void move_command(command &old_ref, command &new_ref);
            void call_refreshed_cb();

        public:
            protocol();
            protocol(const char *if_name, uint32_t refresh_ms = 1000);
            protocol(const protocol &p) = delete;
            protocol(protocol &&p);
            ~protocol();
            
            protocol &operator=(const protocol &p) = delete;
            protocol &operator=(protocol &&p);

            void set_refresh_ms(uint32_t ms);
            void set_refreshed_cb(const std::function<void(void)> &cb);

            friend class command;
    };
}