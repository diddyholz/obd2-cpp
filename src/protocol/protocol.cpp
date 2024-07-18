#include "protocol.h"

#include <iostream>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <net/if.h>
#include <stdexcept>
#include <sys/ioctl.h>

#define UDS_MSG_MAX             1024

#define UDS_RX_SID_OFFSET        0x40
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
        if (this == &p) {
            return;
        }

        if (listener_running) {
            listener_running = false;
            listener_thread.join();
        }

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
        listener_thread.join();

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
        const std::chrono::milliseconds delay = std::chrono::milliseconds(refresh_ms);

        while (listener_running) {
            auto start = std::chrono::steady_clock::now();

            process_commands();
            std::this_thread::sleep_until(start + (delay / 2));

            process_sockets();
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
        std::lock_guard<std::mutex> commands_lock(commands_mutex);

        // Go through each request and send CAN message
        for (auto &p : command_socket_map) {
            command *c = p.first;

            if (!c->refresh) {
                continue;
            }

            process_command(*c);
        }
    }

    void protocol::process_sockets() {
        std::lock_guard<std::mutex> sockets_lock(sockets_mutex);

        // Go through each socket and process incoming message
        for (socket_wrapper &s : sockets) {
            process_socket(s);
        }
    }

    void protocol::process_command(command &c) {
        std::vector<uint8_t> msg_buf = c.get_can_msg();
        
        socket_wrapper &s = command_socket_map.at(&c);
        s.send_msg(msg_buf.data(), msg_buf.size());
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
        uint8_t *data = &buffer[UDS_RES_PID];

        if (sid == UDS_SID_NEGATIVE) {
            nrc = buffer[UDS_RES_NRC];
            sid = buffer[UDS_RES_REJECTED_SID];
        }

        // Go through each request and check if data is for specified request
        // Only check pid if response is positive
        for (auto &p : command_socket_map) {
            command *c = p.first;

            if (c->tx_id != s.tx_id
                || c->rx_id != s.rx_id
                || c->sid != (sid - UDS_RX_SID_OFFSET) 
                || (!c->contains_pid(pid) && nrc == 0)) {
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
                c->update_back_buffer(data, data + size - UDS_RES_PID);
            }

            return;
        }
    }

    void protocol::set_refresh_ms(uint32_t ms) {
        refresh_ms = ms;
    }

    void protocol::set_refreshed_cb(const std::function<void(void)> &cb) {
        std::lock_guard<std::mutex> refreshed_cb_lock(refreshed_cb_mutex);
        refreshed_cb = cb;
    }

    void protocol::add_command(command &c) {
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
    }

    void protocol::remove_command(command &c) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        
        command_socket_map.erase(&c);
    }
    
    void protocol::move_command(command &old_ref, command &new_ref) {
        std::lock_guard<std::mutex> commands_lock(commands_mutex);
        socket_wrapper &socket = command_socket_map.at(&old_ref);

        command_socket_map.erase(&old_ref);
        command_socket_map.emplace(&new_ref, socket);
    }

    void protocol::call_refreshed_cb() {
        std::lock_guard<std::mutex> refreshed_cb_lock(refreshed_cb_mutex);

        if (refreshed_cb) {
            refreshed_cb();
        }
    }
}
