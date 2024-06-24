#include "obd2.h"

#include <cstring>
// #include <sys/ifctl.h>
#include <stdexcept>

#define OBD2_MSG_MAX        1024
#define OBD2_ID_BROADCAST   0x7DF
#define OBD2_ID_START       0x7E0
#define OBD2_ID_END         0x7E7

#define OBD2_RES_SID    0
#define OBD2_RES_PID    1
#define OBD2_RES_DATA   2

namespace obd2 {
    instance::instance(const char *if_name) {
        struct ifreq ifr;
        
        // Get index of specified CAN interface name
        std::strcpy(ifr.ifr_name, if_name);
        ioctl(sock, SIOCGIFINDEX, &ifr);

        this->if_index = ifr.ifr_index;

        // Open sockets for OBD communication
        this->open_default_sockets();

        // Start background response listener thread
        listener_thread = std::thread(instance::response_listener, this);
    }

    instance::~instance() {
        for (request *&r : this->active_requests) {
            delete r;
            r = nullptr;
        }
    }

    void instance::response_listener() {
        for (;;) {
            // Go through each socket and process incoming message
            for (socket_wrapper &s : this->sockets) {
                uint8_t buffer[OBD2_MSG_MAX];
                size_t size = s.read_msg(buffer, sizeof(buffer));

                if (size == 0) {
                    continue;
                }

                uint8_t sid = buffer[OBD2_RES_SID];
                uint8_t pid = buffer[OBD2_RES_PID];
                uint8_t *data = &buffer[OBD2_RES_DATA];

                // Go through each request and check if data is for specified request
                for (request *&r : this->active_requests) {
                    if (r->tx_id != s.tx_id && r->tx_id != OBD2_ID_BROADCAST
                        || r->sid != sid 
                        || r->pid != pid) {
                        continue;
                    }

                    // TODO: What if multiple ECUs respond to same request on broadcast id
                    r->rx_id = s.rx_id;
                    r->get_next_response_buf().assign(data, data + size);
                    r->update_response();

                    if (!r->refresh) {
                        this->active_requests.remove(r);
                        this->prev_requests.emplace_back(r);
                    }
                }
            }
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

    request &instance::add_request(uint32_t tx_id, uint8_t sid, uint16_t pid, bool refresh) {
        // Check if identical request already exists
        for (request *&r : this->active_requests) {
            if (r->tx_id = tx_id && r->sid == sid && r->pid == pid) {
                throw std::invalid_argument("A request with the specified parameters already exists");
            }
        }

        request *new_request = new request(tx_id, sid, pid, refresh);
        this->active_requests.push_back(new_request);

        // TODO: For UDS: Check if socket must be opened

        return *new_request;
    }
}