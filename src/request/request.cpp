#include "request.h"

namespace obd2 {
    request::request(uint32_t can_id, uint8_t sid, uint16_t pid, bool refresh) 
        : tx_id(can_id), sid(sid), pid(pid), refresh(refresh) {
    }

    bool request::operator==(const request &r) const {
        return (this->tx_id == r.tx_id && this->sid == r.sid && this->pid == r.pid);
    }

    void request::update_response() {
        this->response_updated = true;
    }

    std::vector<uint8_t> &request::get_next_response_buf() {
        const size_t next_buf = (this->cur_response_buf + 1) % this->RESPONSE_BUF_COUNT;

        return this->response_bufs[next_buf];
    }
    
    const std::vector<uint8_t> &request::get_current_response_buf() {
        // Check if buffers need to be cycled
        if (response_updated) {
            this->cur_response_buf = (this->cur_response_buf + 1) % this->RESPONSE_BUF_COUNT;
        }

        return this->response_bufs[this->cur_response_buf];
    }
}
