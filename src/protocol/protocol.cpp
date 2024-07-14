#include "protocol.h"

#include <iostream>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <net/if.h>
#include <stdexcept>
#include <sys/ioctl.h>

#define UDS_MSG_MAX             1024

#define UDS_RX_ID_OFFSET        0x40
#define UDS_RES_SID             0x00
#define UDS_RES_PID             0x01
#define UDS_RES_DATA            0x02
#define UDS_RES_REJECTED_SID    0x01
#define UDS_RES_NRC             0x02

#define UDS_SID_NEGATIVE    0x7F

namespace obd2 {
    protocol::protocol(const char *if_name, uint32_t refresh_ms) 
        : refresh_ms(refresh_ms) {
        
        // Get index of specified CAN interface name
        if ((if_index = if_nametoindex(if_name)) == 0) {
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Start background command listener thread
        listener_thread = std::thread(&protocol::command_listener, this);
    }

    void protocol::command_listener() {
        for (;;) {
            process_commands();
            std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms / 2));

            process_sockets();
            std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms / 2));
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

    void protocol::process_commands() {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);

        // Go through each request and send CAN message
        for (command *r : active_commands) {
            process_command(*r);
        }
    }

    void protocol::process_sockets() {
        // Go through each socket and process incoming message
        for (socket_wrapper &s : sockets) {
            process_socket(s);
        }
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

        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        uint8_t nrc = 0;
        uint8_t sid = buffer[UDS_RES_SID];
        uint8_t pid = buffer[UDS_RES_PID];
        uint8_t *data = &buffer[UDS_RES_DATA];

        if (sid == UDS_SID_NEGATIVE) {
            nrc = buffer[UDS_RES_NRC];
            sid = buffer[UDS_RES_REJECTED_SID];
        }

        // Go through each request and check if data is for specified request
        // Only check pid if response is positive
        for (command *c : active_commands) {
            if (c->tx_id != s.tx_id
                || c->rx_id != s.rx_id
                || c->sid != (sid - UDS_RX_ID_OFFSET) 
                || (c->pid != pid && nrc == 0)) {
                continue;
            }

            // When negative response and command status was not OK before,
            // set command to contain error
            if (nrc != 0 && c->response_status != command::status::OK) {
                std::vector neg_data = { nrc };

                c->update_back_buffer(neg_data.data(), neg_data.data() + neg_data.size());
                c->response_status = command::status::ERROR;
            }
            else {
                c->update_back_buffer(data, data + size - UDS_RES_DATA);
            }
            
            if (!c->refresh) {
                stop_command_nolock(*c);
            }

            return;
        }
    }

    void protocol::resume_command_nolock(command &c) {
        size_t initial_size = stopped_commands.size();

        stopped_commands.remove(&c);
        
        if (stopped_commands.size() == initial_size) {
            return;
        }

        c.refresh = true;
        active_commands.emplace_back(&c);
    }

    void protocol::stop_command_nolock(command &c) {
        size_t initial_size = active_commands.size();

        active_commands.remove(&c);
        
        if (active_commands.size() == initial_size) {
            return;
        }

        c.refresh = false;
        stopped_commands.emplace_back(&c);
    }

    void protocol::set_refresh_ms(uint32_t refresh_ms) {
        this->refresh_ms = refresh_ms;
    }

    void protocol::add_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);

        // Check if identical command already exists
        for (command *active_c : active_commands) {
            if (active_c->tx_id == c.tx_id 
                && active_c->rx_id == c.rx_id 
                && active_c->sid == c.sid 
                && active_c->pid == c.pid) {
                throw std::invalid_argument("Command already exists");
            }
        }

        // Check if identical command is currently stopped
        for (command *stopped_c : stopped_commands) {
            if (stopped_c->tx_id == c.tx_id 
                && stopped_c->rx_id == c.rx_id 
                && stopped_c->sid == c.sid 
                && stopped_c->pid == c.pid) {
                throw std::invalid_argument("Command already exists");
            }
        }

        // Open required socket
        open_socket(c.tx_id, c.rx_id);

        active_commands.emplace_back(&c);
    }

    void protocol::remove_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);

        active_commands.remove(&c);
        stopped_commands.remove(&c);
    }
    
    void protocol::move_command(command &old_ref, command &new_ref) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);

        size_t initial_size = active_commands.size();
        active_commands.remove(&old_ref);

        if (active_commands.size() != initial_size) {
            active_commands.emplace_back(&new_ref);
            return;
        }

        initial_size = stopped_commands.size();
        stopped_commands.remove(&old_ref);

        if (stopped_commands.size() != initial_size) {
            stopped_commands.emplace_back(&new_ref);
            return;
        }
    }

    void protocol::resume_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        resume_command_nolock(c);
    }

    void protocol::stop_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        stop_command_nolock(c);
    }
}
