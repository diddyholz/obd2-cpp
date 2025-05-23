#include "protocol.h"

#include <iostream>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <net/if.h>
#include <stdexcept>
#include <sys/ioctl.h>

#include "command/command_backend/command_backend.h"

#define UDS_MSG_MAX             1024

#define UDS_RX_SID_OFFSET       0x40
#define UDS_RES_SID             0x00
#define UDS_RES_PID             0x01
#define UDS_RES_DATA            0x02
#define UDS_RES_REJECTED_SID    0x01
#define UDS_RES_NRC             0x02

#define UDS_SID_NEGATIVE    0x7F

namespace obd2 {
    protocol::protocol() {}

    protocol::protocol(const char *if_name, uint32_t refresh_ms) 
        : refresh_ms(refresh_ms) {
        
        // Get index of specified CAN interface name
        if ((if_index = if_nametoindex(if_name)) == 0) {
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Start background command listener thread
        listener_running = true;
        listener_thread = std::thread(&protocol::command_listener, this);
    }

    protocol::protocol(protocol &&p) {
        listener_running.store(p.listener_running);
        refresh_ms.store(p.refresh_ms);

        if_index = p.if_index;

        if (listener_running) {
            listener_thread = std::thread(&protocol::command_listener, this);
        }

        refreshed_cb = p.refreshed_cb;

        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        std::lock_guard<std::mutex> sockets_lock(sockets_mutex);
        std::lock_guard<std::mutex> refreshed_cb_lock(refreshed_cb_mutex);

        for (auto &p : command_socket_map) {
            p.first->parent = nullptr;
        }

        command_socket_map = std::move(p.command_socket_map);
        sockets = std::move(p.sockets);
        refreshed_cb = std::move(p.refreshed_cb);

        for (auto &p : command_socket_map) {
            p.first->parent = this;
        }
    }

    protocol::~protocol() {
        listener_running = false;

        // Try to join listener thread if it is running
        try {
            listener_thread.join();
        }
        catch(const std::system_error &e) {
            // Ignore error if thread is not joinable
        }

        for (auto &p : command_socket_map) {
            p.first->parent = nullptr;
        }
    }

    protocol &protocol::operator=(protocol &&p) {
        if (this == &p) {
            return *this;
        }

        if (listener_running) {
            listener_running = false;
            listener_thread.join();
        }

        refresh_ms.store(p.refresh_ms);
        listener_running.store(p.listener_running);
        
        if_index = p.if_index;

        if (listener_running) {
            listener_thread = std::thread(&protocol::command_listener, this);
        }

        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        std::lock_guard<std::mutex> sockets_lock(sockets_mutex);
        std::lock_guard<std::mutex> refreshed_cb_lock(refreshed_cb_mutex);

        for (auto &p : command_socket_map) {
            p.first->parent = nullptr;
        }

        command_socket_map = std::move(p.command_socket_map);
        sockets = std::move(p.sockets);
        refreshed_cb = std::move(p.refreshed_cb);

        for (auto &p : command_socket_map) {
            p.first->parent = this;
        }

        return *this;
    }

    void protocol::command_listener() {
        while (listener_running) {
            auto delay = std::chrono::milliseconds(refresh_ms);
            auto start = std::chrono::steady_clock::now();

            // Reset flag for this iteration
            next_recieved_response = false;

            process_commands();

            // Process sockets once again, in case the command queue was empty but there are still responses to be received
            process_sockets();

            // Update flag after processing sockets
            recieved_response = next_recieved_response.load();

            call_refreshed_cb();

            std::this_thread::sleep_until(start + delay);
        }
    }

    socket_wrapper &protocol::get_socket(uint32_t tx_id, uint32_t rx_id) {
        std::lock_guard<std::mutex> sockets_lock(sockets_mutex);

        // Check if socket already exists
        for (socket_wrapper &s : sockets) {
            if (s.tx_id == tx_id && s.rx_id == rx_id) {
                return s;
            }
        }

        return sockets.emplace_back(tx_id, rx_id, if_index);
    }

    void protocol::process_commands() {
        std::queue<std::reference_wrapper<command_backend>> new_command_queue;
        
        while (!command_queue.empty()) {
            command_backend &c = command_queue.front();
            command_queue.pop();

            process_command(c);

            auto start = std::chrono::steady_clock::now();
            uint32_t timeout = command_process_timeout;

            // If no response is expected, lower timeout
            if (c.response_status == cmd_status::NO_RESPONSE) {
                timeout = no_response_command_timeout;
            }

            bool response_received = false;

            // If for what ever reason the socket is not in the map, add it
            if (command_socket_map.find(&c) == command_socket_map.end()) {
                command_socket_map.emplace(&c, get_socket(c.tx_id, c.rx_id));
            }

            // Process incoming data from sockets until response is received
            while (
                !(response_received = process_socket(command_socket_map.at(&c), &c)) 
                && (std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(timeout)
            ) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            // !!! This is disabled for now, as it is breaking the simulator behaviour

            // if (c.response_status == cmd_status::ERROR) {
            //     return;
            // }

            if (!response_received) {
                c.response_status = cmd_status::NO_RESPONSE;
                c.response_buffer = {};
            }

            new_command_queue.push(c);
        }

        command_queue.swap(new_command_queue);
    }

    void protocol::process_command(command_backend &c) {
        std::vector<uint8_t> msg_buf = c.get_can_msg();
        
        socket_wrapper &s = command_socket_map.at(&c);
        s.send_msg(msg_buf.data(), msg_buf.size());
    }

    bool protocol::process_sockets() {
        std::lock_guard<std::mutex> sockets_lock(sockets_mutex);

        // Go through each socket and process incoming messages
        for (socket_wrapper &s : sockets) {
            process_socket(s);
        }

        return false;
    }

    bool protocol::process_socket(socket_wrapper &s, command_backend *command_to_check) {
        uint8_t buffer[UDS_MSG_MAX];
        size_t size = s.read_msg(buffer, sizeof(buffer));
        bool cmd_response = false;

        if (size == 0) {
            return false;
        }

        next_recieved_response = true;
        
        uint8_t nrc = 0; // Negative response code
        uint8_t sid = buffer[UDS_RES_SID];
        uint8_t pid = buffer[UDS_RES_PID];
        uint8_t *data = &buffer[UDS_RES_PID];
        std::list<std::reference_wrapper<command_backend>> to_complete;
        bool is_dtc = false;

        // Check if response is negative or dtc response
        if (sid == UDS_SID_NEGATIVE) {
            nrc = buffer[UDS_RES_NRC];
            sid = buffer[UDS_RES_REJECTED_SID] + UDS_RX_SID_OFFSET;
        }
        else if (sid == 0x03 + UDS_RX_SID_OFFSET 
            || sid == 0x07 + UDS_RX_SID_OFFSET
            || sid == 0x0A + UDS_RX_SID_OFFSET) {
            // TODO: Full UDS implementation
            // => This should not be checked in this layer
            is_dtc = true;
        }

        commands_mutex.lock();

        // Go through each request and check if data is for specified request
        // Only check pid if response is positive
        for (auto &p : command_socket_map) {
            command_backend *cmd = p.first;

            if (cmd->tx_id != s.tx_id
                || cmd->rx_id != s.rx_id
                || cmd->sid != (sid - UDS_RX_SID_OFFSET) 
                || (nrc == 0 && !is_dtc && !cmd->contains_pid(pid))) {
                continue;
            }

            // When negative response and command status was not OK before,
            // set command to contain error
            if (nrc != 0) {
                if (cmd->response_status == cmd_status::OK) {
                    continue;
                }

                std::vector neg_data = { nrc };

                cmd->update_back_buffer(neg_data.data(), neg_data.data() + neg_data.size());
                cmd->response_status = cmd_status::ERROR;
            }
            else {
                cmd->update_back_buffer(data, data + size - UDS_RES_PID);
            }

            // If command is not set to be refreshed, complete it after loop
            if (!cmd->refresh) {
                to_complete.push_back(*cmd);
            }

            // If the function should check for a specific command and the commands response is recieved, 
            // set the corresponding flag
            if (command_to_check && cmd == command_to_check) {
                cmd_response = true;
            }
        }

        commands_mutex.unlock();

        // Complete commands that are not set to be refreshed
        for (auto &cmd : to_complete) {
            cmd.get().complete();
        }

        return cmd_response;
    }

    void protocol::set_refresh_ms(uint32_t ms) {
        refresh_ms = ms;
    }

    void protocol::set_refreshed_cb(const std::function<void(void)> &cb) {
        std::lock_guard<std::mutex> refreshed_cb_lock(refreshed_cb_mutex);
        refreshed_cb = cb;
    }

    uint32_t protocol::get_refresh_ms() const {
        return refresh_ms.load();
    }

    bool protocol::recieved_any_response() {
        return recieved_response.load();
    }

    void protocol::add_command(command_backend &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);

        // Check if identical command already exists
        for (auto &p : command_socket_map) {
            if (p.first->tx_id == c.tx_id 
                && p.first->rx_id == c.rx_id
                && p.first->sid == c.sid
                && p.first->pids == c.pids) {
                throw std::invalid_argument("Command already exists");
            }
        }

        // Get required socket
        socket_wrapper &socket = get_socket(c.tx_id, c.rx_id);
        command_socket_map.emplace(&c, socket);

        // Add command to queue
        if (c.refresh) {
            command_queue.push(c);
        }
    }

    void protocol::remove_command(command_backend &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        
        command_socket_map.erase(&c);

        // Remove command from queue
        std::queue<std::reference_wrapper<command_backend>> new_command_queue;

        while (!command_queue.empty()) {
            command_backend &tmp = command_queue.front();
            command_queue.pop();

            if (&tmp != &c) {
                new_command_queue.push(tmp);
            }
        }

        command_queue.swap(new_command_queue);
        c.parent = nullptr;
    }
    
    void protocol::move_command(command_backend &old_ref, command_backend &new_ref) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        socket_wrapper &socket = command_socket_map.at(&old_ref);

        command_socket_map.erase(&old_ref);
        command_socket_map.emplace(&new_ref, socket);

        old_ref.parent = nullptr;
    }

    void protocol::call_refreshed_cb() {
        std::lock_guard<std::mutex> refreshed_cb_lock(refreshed_cb_mutex);

        if (refreshed_cb) {
            refreshed_cb();
        }
    }
}
