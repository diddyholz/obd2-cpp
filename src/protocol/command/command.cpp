#include "command.h"

#include "../protocol.h"

namespace obd2 {
    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, bool refresh, protocol &parent) 
        : parent(&parent), tx_id(tx_id), rx_id(rx_id), sid(sid), pid(pid), refresh(refresh) { }

    command::command(command &c) 
        : parent(c.parent), tx_id(c.tx_id), rx_id(c.rx_id), sid(c.sid), pid(c.pid), refresh(c.refresh) {
        if (this == &c) {
            return;
        }

        response_bufs[cur_response_buf] = c.get_current_buf();
    }

    command::command(command &&c)
        : parent(c.parent), tx_id(c.tx_id), rx_id(c.rx_id), sid(c.sid), pid(c.pid), refresh(c.refresh) {
        if (this == &c) {
            return;
        }

        response_bufs[cur_response_buf] = std::move(c.get_current_buf());
    }

    command &command::operator=(command &c) {
        if (this == &c) {
            return *this;
        }

        parent = c.parent;
        tx_id = c.tx_id;
        rx_id = c.rx_id;
        sid = c.sid;
        pid = c.pid;
        refresh = c.refresh;

        response_bufs[cur_response_buf] = c.get_current_buf();

        return *this;
    }

    bool command::operator==(const command &r) const {
        return (parent == r.parent 
            && tx_id == r.tx_id 
            && rx_id == r.rx_id 
            && sid == r.sid 
            && pid == r.pid);
    }

    std::vector<uint8_t> command::get_can_msg() const {        
        std::vector<uint8_t> buf = { sid, static_cast<uint8_t>(pid & 0xFF) };

        // Add second byte if PID is 16 bits
        if (pid > 0xFF) {
            buf.push_back(static_cast<uint8_t>(pid >> 8));
        }

        return buf;
    }

    void command::resume() {
        parent->resume_command(*this);
    }

    void command::stop() {
        parent->stop_command(*this);
    }

    uint32_t command::get_tx_id() const {
        return tx_id;
    }

    uint32_t command::get_rx_id() const {
        return rx_id;
    }
    
    uint8_t command::get_sid() const {
        return sid;
    }
    
    uint16_t command::get_pid() const {
        return pid;
    }

    void command::update_next_buf(const uint8_t *start, const uint8_t *end) {
        std::lock_guard<std::mutex> response_bufs_lock(response_bufs_mutex);

        const size_t next_buf = (cur_response_buf + 1) % RESPONSE_BUF_COUNT;
        response_bufs[next_buf].assign(start, end);
        response_updated = true;
    }
    
    const std::vector<uint8_t> &command::get_current_buf() {
        // Check if buffers need to be cycled
        if (response_updated) {
            // Wait before switching if the next buffer is currently beeing written to
            std::lock_guard<std::mutex> response_bufs_lock(response_bufs_mutex);

            cur_response_buf = (cur_response_buf + 1) % RESPONSE_BUF_COUNT;
            response_updated = false;
        }

        return response_bufs[cur_response_buf];
    }
}
