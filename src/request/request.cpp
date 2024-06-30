#include "request.h"

namespace obd2 {
    request::request(uint32_t can_id, uint8_t sid, uint16_t pid, bool refresh) 
        : tx_id(can_id), sid(sid), pid(pid), refresh(refresh) { }

    bool request::operator==(const request &r) const {
        return (this->tx_id == r.tx_id && this->sid == r.sid && this->pid == r.pid);
    }

    void request::get_can_msg(std::vector<uint8_t> &buf) {        
        buf = { this->sid, (uint8_t)(this->pid & 0xFF), (uint8_t)(this->pid >> 8) };

        // If pid is only 8 bits, cut off second PID byte
        if (this->pid < 0x100) {
            buf.resize(buf.size() - 1);
        }
    }

    uint32_t request::get_tx_id() const {
        return this->tx_id;
    }
    
    uint8_t request::get_sid() const {
        return this->sid;
    }
    
    uint16_t request::get_pid() const {
        return this->pid;
    }

    std::list<ecu> &request::get_ecus() {
        return this->ecus;
    }

    ecu &request::get_ecu_by_id(uint32_t rx_id) {
        for (ecu &e : this->ecus) {
            if (e.get_rx_id() == rx_id) {
                return e;
            }
        }

        // If no ECU matching the ID was found, create a new ECU entry
        return this->ecus.emplace_back(rx_id);
    }
}
