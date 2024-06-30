#include "ecu.h"

namespace obd2 {
    ecu::ecu(uint32_t rx_id) : rx_id(rx_id) { }

    ecu::ecu(ecu &e) : rx_id(e.rx_id) {
        this->response_bufs[this->cur_response_buf] = e.get_current_response_buf();
    }

    ecu::ecu(ecu &&e) : rx_id(e.rx_id) {
        this->response_bufs[this->cur_response_buf] = std::move(e.get_current_response_buf());
    }

    ecu &ecu::operator=(ecu &e) {
        this->response_bufs[this->cur_response_buf] = e.get_current_response_buf();

        return *this;
    }

    void ecu::update_next_response_buf(const uint8_t *start, const uint8_t *end) {
        std::lock_guard<std::mutex> response_bufs_lock(this->response_bufs_mutex);

        const size_t next_buf = (this->cur_response_buf + 1) % this->RESPONSE_BUF_COUNT;
        this->response_bufs[next_buf].assign(start, end);
        this->response_updated = true;
    }
    
    const std::vector<uint8_t> &ecu::get_current_response_buf() {
        // Check if buffers need to be cycled
        if (response_updated) {
            // Wait before switching if the next buffer is currently beeing written to
            std::lock_guard<std::mutex> response_bufs_lock(this->response_bufs_mutex);

            this->cur_response_buf = (this->cur_response_buf + 1) % this->RESPONSE_BUF_COUNT;
            this->response_updated = false;
        }

        return this->response_bufs[this->cur_response_buf];
    }

    uint32_t ecu::get_rx_id() {
        return this->rx_id;
    }
}
