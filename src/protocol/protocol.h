#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "command/command.h"
#include "socket_wrapper/socket_wrapper.h"

namespace obd2 {
    class command_backend;

    class protocol {
        private:
            std::unordered_map<command_backend *, std::reference_wrapper<socket_wrapper>> command_socket_map;
            std::queue<std::reference_wrapper<command_backend>> command_queue;
            std::mutex commands_mutex;

            std::vector<socket_wrapper> sockets;
            std::mutex sockets_mutex;
            
            uint32_t command_process_timeout = 1000;
            uint32_t no_response_command_timeout = 1;

            std::atomic<bool> recieved_response = false;
            std::atomic<bool> next_recieved_response = false;

            unsigned int if_index;
            std::atomic<uint32_t> refresh_ms;
            std::thread listener_thread;
            std::atomic<bool> listener_running = false;

            std::function<void(void)> refreshed_cb;
            std::mutex refreshed_cb_mutex;

            void command_listener();
            void process_commands();
            bool process_sockets();
            bool process_socket(socket_wrapper &s, command_backend *command_to_check = nullptr);
            void process_command(command_backend &c);
            socket_wrapper &get_socket(uint32_t tx_id, uint32_t rx_id);
            void add_command(command_backend &c);
            void remove_command(command_backend &c);
            void move_command(command_backend &old_ref, command_backend &new_ref);
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
            bool recieved_any_response();

            uint32_t get_refresh_ms() const;

            friend class command_backend;
    };
}