#include "protocol.h"

#include <iostream>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <net/if.h>
#include <stdexcept>
#include <sys/ioctl.h>

#define UDS_MSG_MAX         1024

#define UDS_RX_ID_OFFSET    0x40
#define UDS_RES_SID         0
#define UDS_RES_PID         1
#define UDS_RES_DATA        2

namespace obd2 {
    protocol::protocol(const char *if_name, uint32_t refresh_ms) 
        : refresh_ms(refresh_ms) {
        
        // Get index of specified CAN interface name
        if (((this->if_index = if_nametoindex(if_name)) == 0)) {
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Start background command listener thread
        listener_thread = std::thread(&protocol::command_listener, this);
    }

    protocol::~protocol() {
        for (command *&r : this->active_commands) {
            delete r;
            r = nullptr;
        }

        for (command *&r : this->stopped_commands) {
            delete r;
            r = nullptr;
        }
    }

    void protocol::command_listener() {
        for (;;) {
            // Go through each request and send CAN message
            for (command *&r : this->active_commands) {
                this->process_command(*r);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(this->refresh_ms / 2));

            // Go through each socket and process incoming message
            for (socket_wrapper &s : this->sockets) {
                this->process_socket(s);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(this->refresh_ms / 2));
        }
    }

    void protocol::open_socket(uint32_t tx_id, uint32_t rx_id) {
        // Check if socket already exists
        for (socket_wrapper &s : this->sockets) {
            if (s.tx_id == tx_id && s.rx_id == rx_id) {
                return;
            }
        }

        this->sockets.emplace_back(tx_id, rx_id, this->if_index);
    }

    void protocol::process_command(command &c) {
        std::vector<uint8_t> msg_buf = c.get_can_msg();

        // Find socket to use for this command
        for (socket_wrapper &s : this->sockets) {
            if (s.tx_id == c.tx_id && s.rx_id == c.rx_id) {
                s.send_msg(msg_buf.data(), msg_buf.size());
            }
        }      
    }

    void protocol::process_socket(socket_wrapper &s) {
        uint8_t buffer[UDS_MSG_MAX];
        size_t size = s.read_msg(buffer, sizeof(buffer));

        if (size == 0) {
            return;
        }

        uint8_t sid = buffer[UDS_RES_SID];
        uint8_t pid = buffer[UDS_RES_PID];
        uint8_t *data = &buffer[UDS_RES_DATA];

        // Go through each request and check if data is for specified request
        for (command *&c : this->active_commands) {
            if (c->tx_id != s.tx_id
                || c->rx_id != s.rx_id
                || c->sid != (sid - UDS_RX_ID_OFFSET) 
                || c->pid != pid) {
                continue;
            }

            c->update_next_buf(data, data + size - UDS_RES_DATA);

            if (!c->refresh) {
                this->stop_command(*c);
            }
        }
    }

    void protocol::resume_command_nolock(command &c) {
        size_t initial_size = stopped_commands.size();

        this->stopped_commands.remove(&c);
        
        if (this->stopped_commands.size() == initial_size) {
            return;
        }

        c.refresh = true;
        this->active_commands.emplace_back(&c);
    }

    void protocol::stop_command_nolock(command &c) {
        size_t initial_size = active_commands.size();

        this->active_commands.remove(&c);
        
        if (this->active_commands.size() == initial_size) {
            return;
        }

        c.refresh = false;
        this->stopped_commands.emplace_back(&c);
    }

    void protocol::set_refresh_ms(uint32_t refresh_ms) {
        this->refresh_ms = refresh_ms;
    }

    command &protocol::add_command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, bool refresh) {
        std::lock_guard<std::mutex> commands_lock(this->commands_mutex);

        // Check if identical command already exists
        for (command *&c : this->active_commands) {
            if (c->tx_id == tx_id && c->rx_id == rx_id && c->sid == sid && c->pid == pid) {
                c->refresh = (refresh)? true : c->refresh;
                return *c;
            }
        }

        // Check if identical command has existed before
        for (command *&c : this->stopped_commands) {
            if (c->tx_id == tx_id && c->rx_id == rx_id && c->sid == sid && c->pid == pid) {
                this->resume_command_nolock(*c);
                c->refresh = (refresh)? true : c->refresh;
                return *c;
            }
        }

        // Open required socket
        this->open_socket(tx_id, rx_id);

        command *new_command = new command(tx_id, rx_id, sid, pid, refresh, *this);
        this->active_commands.push_back(new_command);

        return *new_command;
    }

    void protocol::resume_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(this->commands_mutex);
        this->resume_command_nolock(c);
    }

    void protocol::stop_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(this->commands_mutex);
        this->stop_command_nolock(c);
    }
}
