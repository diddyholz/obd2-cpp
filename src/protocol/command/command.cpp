#include "command.h"

#include "../protocol.h"

namespace obd2 {
    command::command() : parent(nullptr) {}

    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, protocol &parent, bool refresh) 
        : command(tx_id, rx_id, sid, std::vector<uint16_t>({ pid }), parent, refresh) {}

    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids, protocol &parent, bool refresh) 
        : parent(&parent), tx_id(tx_id), rx_id(rx_id), sid(sid), pids(pids), refresh(refresh) { 
        parent.add_command(*this);
        parent.process_command(*this);
    }

    command::command(command &&c) : parent(c.parent), tx_id(c.tx_id), rx_id(c.rx_id), sid(c.sid) {
        refresh.store(c.refresh);

        if (c.parent) {	
            parent->move_command(c, *this);
        }

        response_buffer = std::move(c.get_buffer());
        response_status.store(c.response_status);
        response_updated = false;

        pids = std::move(c.get_pids());
    }

    command::~command() {
        complete();
    }

    command &command::operator=(command &&c) {
        if (this == &c) {
            return *this;
        }

        if (parent) {
            parent->remove_command(*this);
        }

        parent = c.parent;
        tx_id = c.tx_id;
        rx_id = c.rx_id;
        sid = c.sid;
        refresh.store(c.refresh);

        if (c.parent) {
            parent->move_command(c, *this);
        }

        std::lock_guard<std::mutex> response_bufs_lock(response_bufs_mutex);
        std::lock_guard<std::mutex> pids_lock(pids_mutex);

        response_buffer = std::move(c.get_buffer());
        response_status.store(c.response_status);
        response_updated = false;

        pids = std::move(c.get_pids());

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
            std::lock_guard<std::mutex> response_bufs_lock(response_bufs_mutex);

            response_buffer = std::move(back_buffer);
            back_buffer = std::vector<uint8_t>();

            response_updated = false;
        }

        return response_buffer;
    }

    void command::complete() {
        if (parent) {
            parent->remove_command(*this);
        }
    }

    void command::resume() {
        check_parent();

        refresh = true;
    }

    void command::stop() {
        check_parent();

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
    
    const std::vector<uint16_t> &command::get_pids() {
        std::lock_guard<std::mutex> pids_lock(pids_mutex);
        return pids;
    }

    void command::set_pids(const std::vector<uint16_t> &pids) {
        std::lock_guard<std::mutex> pids_lock(pids_mutex);
        this->pids = pids;
    }

    void command::add_pid(uint16_t pid) {
        std::lock_guard<std::mutex> pids_lock(pids_mutex);
        pids.push_back(pid);
    }

    void command::remove_pid(uint16_t pid) {
        std::lock_guard<std::mutex> pids_lock(pids_mutex);
        pids.erase(std::find(pids.begin(), pids.end(), pid));
    }
    
    bool command::contains_pid(uint16_t pid) {
        std::lock_guard<std::mutex> pids_lock(pids_mutex);
        return std::find(pids.begin(), pids.end(), pid) != pids.end();
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
        if (!parent) {
            throw std::runtime_error("Command is completed or has no parent");
        }
    }

    std::vector<uint8_t> command::get_can_msg() {        
        std::vector<uint8_t> buf = { sid };

        for (uint16_t pid : pids) {
            buf.push_back(static_cast<uint8_t>(pid));
            
            // Add second byte if PID is 16 bits
            if (pid > 0xFF) {
                buf.push_back(static_cast<uint8_t>(pid >> 8));
            }
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
