#include "ecu.h"

namespace obd2 {
    ecu::ecu(uint32_t rx_id) : rx_id(rx_id) {

    }

    void ecu::update_response() {
        this->response_updated = true;
    }

    std::vector<uint8_t> &ecu::get_next_response_buf() {
        const size_t next_buf = (this->cur_response_buf + 1) % this->RESPONSE_BUF_COUNT;

        return this->response_bufs[next_buf];
    }
    
    const std::vector<uint8_t> &ecu::get_current_response_buf() {
        // Check if buffers need to be cycled
        if (response_updated) {
            this->cur_response_buf = (this->cur_response_buf + 1) % this->RESPONSE_BUF_COUNT;
        }

        return this->response_bufs[this->cur_response_buf];
    }

    uint32_t ecu::get_rx_id() {
        return this->rx_id;
    }
}
