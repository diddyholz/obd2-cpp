#include "../include/obd2.h"

#include <iostream>
#include <cstring>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <chrono>

#define OBD2_MSG_MAX        1024
#define OBD2_ID_BROADCAST   0x7DF
#define OBD2_ID_START       0x7E0
#define OBD2_ID_END         0x7E7

#define OBD2_RES_SID    0
#define OBD2_RES_PID    1
#define OBD2_RES_DATA   2

namespace obd2 {
    instance::instance(const char *if_name, uint32_t refresh_ms) 
        : refresh_ms(refresh_ms) {
        
        // Get index of specified CAN interface name
        if (((this->if_index = if_nametoindex(if_name)) == 0)) {
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Open sockets for OBD communication
        this->open_default_sockets();

        // Start background response listener thread
        listener_thread = std::thread(&instance::response_listener, this);
    }

    instance::~instance() {
        for (request *&r : this->active_requests) {
            delete r;
            r = nullptr;
        }

        for (request *&r : this->prev_requests) {
            delete r;
            r = nullptr;
        }
    }

    void instance::response_listener() {
        for (;;) {
            // Go through each request and send CAN message
            for (request *&r : this->active_requests) {
                this->process_request(*r);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(this->refresh_ms / 2));

            // Go through each socket and process incoming message
            for (socket_wrapper &s : this->sockets) {
                this->process_socket(s);
            }

            this->remove_completed_requests();

            std::this_thread::sleep_for(std::chrono::milliseconds(this->refresh_ms / 2));
        }
    }

    void instance::open_default_sockets() {
        // Open sockets for each ECU (0x7E0 to 0x7E7)
        for (uint32_t tx = OBD2_ID_START; tx <= OBD2_ID_END; tx++) {
            this->sockets.emplace_back(tx, tx + 8, this->if_index);
        }

        // Open socket for broadcast (0x7DF)
        // An rx id is not necessary, as this is socket will not be used to read data
        // and OBD queries sent will also never exceed ISO-TP single frame size
        this->sockets.emplace_back(OBD2_ID_BROADCAST, 0, this->if_index);
    }

    void instance::process_request(request &r) {
        std::vector<uint8_t> msg_buf;
        r.get_can_msg(msg_buf);

        // Find socket to use for this request
        for (socket_wrapper &s : this->sockets) {
            // TODO: What if no socket is found
            if (s.tx_id == r.tx_id) {
                s.send_msg(msg_buf.data(), msg_buf.size());
            }
        }      
    }

    void instance::process_socket(socket_wrapper &s) {
        uint8_t buffer[OBD2_MSG_MAX];
        size_t size = s.read_msg(buffer, sizeof(buffer));

        if (size == 0) {
            return;
        }

        uint8_t sid = buffer[OBD2_RES_SID];
        uint8_t pid = buffer[OBD2_RES_PID];
        uint8_t *data = &buffer[OBD2_RES_DATA];

        // Go through each request and check if data is for specified request
        for (request *&r : this->active_requests) {
            if ((r->tx_id != s.tx_id && r->tx_id != OBD2_ID_BROADCAST)
                || r->sid != sid 
                || r->pid != pid) {
                continue;
            }

            ecu &responding_ecu = r->get_ecu_by_id(s.rx_id);

            responding_ecu.update_next_response_buf(data, data + size - OBD2_RES_DATA);
        }
    }

    void instance::remove_completed_requests() {
        std::lock_guard<std::mutex> requests_lock(this->requests_mutex);

        for (request *&r : this->active_requests) {
            if (r->refresh || r->ecus.size() == 0) {
                continue;
            }

            this->prev_requests.emplace_back(r);
            this->active_requests.remove(r);
        }
    }

    void instance::set_refresh_ms(uint32_t refresh_ms) {
        this->refresh_ms = refresh_ms;
    }

    request &instance::add_request(uint32_t tx_id, uint8_t sid, uint16_t pid, bool refresh) {
        std::lock_guard<std::mutex> requests_lock(this->requests_mutex);

        // Check if identical request already exists
        for (request *&r : this->active_requests) {
            if (r->tx_id == tx_id && r->sid == sid && r->pid == pid) {
                throw std::invalid_argument("A request with the specified parameters already exists");
            }
        }

        request *new_request = new request(tx_id, sid, pid, refresh);
        this->active_requests.push_back(new_request);

        // TODO: For UDS: Check if socket must be opened

        return *new_request;
    }
}