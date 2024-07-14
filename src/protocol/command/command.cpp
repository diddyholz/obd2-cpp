#include "command.h"

#include "../protocol.h"

namespace obd2 {
    command::command() {}

    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, bool refresh, protocol &parent) 
        : parent(&parent), tx_id(tx_id), rx_id(rx_id), sid(sid), pid(pid), refresh(refresh) { 
        parent.add_command(*this);
        parent.process_command(*this);
    }

    command::command(command &&c) {
        if (this == &c) {
            return;
        }

        if (parent != nullptr) {
            parent->remove_command(*this);
        }

        parent = c.parent;
        tx_id = c.tx_id;
        rx_id = c.rx_id;
        sid = c.sid;
        pid = c.pid;
        refresh = c.refresh;

        c.parent = nullptr;

        if (parent != nullptr) {	
            parent->move_command(c, *this);
        }

        response_buffer = std::move(c.get_buffer());
    }

    command::~command() {
        if (parent != nullptr) {
            parent->remove_command(*this);
        }
    }

    command &command::operator=(command &&c) {
        if (this == &c) {
            return *this;
        }

        if (parent != nullptr) {
            parent->remove_command(*this);
        }

        parent = c.parent;
        tx_id = c.tx_id;
        rx_id = c.rx_id;
        sid = c.sid;
        pid = c.pid;
        refresh = c.refresh;

        c.parent = nullptr;

        if (parent != nullptr) {
            parent->move_command(c, *this);
        }

        response_buffer = std::move(c.get_buffer());

        return *this;
    }

    bool command::operator==(const command &c) const {
        // There should only be one instance of each command, the constructor makes sure of that.
        // Therefore, comparing the addresses is enough. Famous last words.
        return &c == this;
    }

    command::status command::get_response_status() {
        return response_status;
    }
    
    const std::vector<uint8_t> &command::get_buffer() {
        // Check if buffers need to be cycled
        if (response_updated) {
            // Wait before switching if the next buffer is currently beeing written to
            std::lock_guard<std::mutex> response_bufs_lock(response_bufs_mutex);

            response_buffer = std::move(back_buffer);
            back_buffer = std::vector<uint8_t>();

            response_updated = false;
        }

        return response_buffer;
    }

    void command::resume() {
        refresh = true;
    }

    void command::stop() {
        refresh = false;
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

    command::status command::wait_for_response(uint32_t timeout_ms, uint32_t sample_us) {
        check_parent();

        auto start = std::chrono::steady_clock::now();

        while (response_status == NO_RESPONSE 
            && (std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(timeout_ms)) {
            std::this_thread::sleep_for(std::chrono::microseconds(sample_us));
        }

        return response_status;
    }

    void command::check_parent() {
        if (parent == nullptr) {
            throw std::runtime_error("Command has no parent");
        }
    }

    std::vector<uint8_t> command::get_can_msg() {        
        std::vector<uint8_t> buf = { sid, static_cast<uint8_t>(pid & 0xFF) };

        // Add second byte if PID is 16 bits
        if (pid > 0xFF) {
            buf.push_back(static_cast<uint8_t>(pid >> 8));
        }

        return buf;
    }

    void command::update_back_buffer(const uint8_t *start, const uint8_t *end) {
        if (!refresh && response_status != NO_RESPONSE) {
            return;
        }

        std::lock_guard<std::mutex> response_bufs_lock(response_bufs_mutex);

        back_buffer.assign(start, end);
        response_updated = true;
        response_status = OK;
    }
}
