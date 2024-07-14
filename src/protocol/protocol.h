#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <thread>
#include <mutex>

#include "command/command.h"
#include "socket_wrapper/socket_wrapper.h"

namespace obd2 {
    class protocol {
        private:
            // TODO: Use mapping from socket to command for faster lookup
            std::list<command *> active_commands;
            std::list<command *> stopped_commands;
            std::mutex commands_mutex;

            std::vector<socket_wrapper> sockets;

            unsigned int if_index;
            uint32_t refresh_ms;
            std::thread listener_thread;

            void command_listener();
            void process_commands();
            void process_sockets();
            void process_command(command &c);
            void process_socket(socket_wrapper &s);
            void open_socket(uint32_t tx_id, uint32_t rx_id);
            void add_command(command &c);
            void remove_command(command &c);
            void move_command(command &old_ref, command &new_ref);
            void resume_command_nolock(command &c);
            void stop_command_nolock(command &c);
            void resume_command(command &c);
            void stop_command(command &c);

        public:
            protocol(const char *if_name, uint32_t refresh_ms = 1000);
            protocol(const protocol &p) = delete;
            protocol(const protocol &&p) = delete;
            
            protocol &operator=(const protocol &p) = delete;
            protocol &operator=(protocol &p) = delete;

            void set_refresh_ms(uint32_t refresh_ms);

            friend class command;
    };
}