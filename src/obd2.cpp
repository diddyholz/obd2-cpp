#include "../include/obd2.h"

#include <algorithm>
#include <future>
#include <stdexcept>

namespace obd2 {
    obd2::obd2() {}

    obd2::obd2(const char *if_name, uint32_t refresh_ms, bool enable_pid_chaining) 
        : protocol_instance(if_name, refresh_ms), enable_pid_chaining(enable_pid_chaining) { }

    obd2::obd2(obd2 &&o) {
        protocol_instance = std::move(o.protocol_instance);
        req_combinations = std::move(o.req_combinations);
        req_combinations_map = std::move(o.req_combinations_map);
        ecus = std::move(o.ecus);
        vehicle = o.vehicle;
        enable_pid_chaining = o.enable_pid_chaining;

        for (auto &p : req_combinations_map) {
            p.first->parent = this;
        }

        o.connection_mutex.lock();
        last_connection_active = o.last_connection_active.load();
        o.connection_mutex.unlock();
    }

    obd2::~obd2() {
        for (auto &p : req_combinations_map) {
            p.first->parent = nullptr;
        }
    }

    obd2 &obd2::operator=(obd2 &&o) {
        if (this == &o) {
            return *this;
        }

        for (auto &p : req_combinations_map) {
            p.first->parent = nullptr;
        }

        protocol_instance = std::move(o.protocol_instance);
        req_combinations = std::move(o.req_combinations);
        req_combinations_map = std::move(o.req_combinations_map);
        ecus = std::move(o.ecus);
        vehicle = o.vehicle;
        enable_pid_chaining = o.enable_pid_chaining;

        for (auto &p : req_combinations_map) {
            p.first->parent = this;
        }

        o.connection_mutex.lock();
        last_connection_active = o.last_connection_active.load();
        o.connection_mutex.unlock();

        return *this;
    }

    void obd2::set_refreshed_cb(const std::function<void(void)> &cb) {
        protocol_instance.set_refreshed_cb(cb);
    }

    void obd2::set_refresh_ms(uint32_t refresh_ms) {
        protocol_instance.set_refresh_ms(refresh_ms);
    }

    void obd2::set_enable_pid_chaining(bool enable_pid_chaining) {
        this->enable_pid_chaining = enable_pid_chaining;
    }

    uint32_t obd2::get_refresh_ms() const {
        return protocol_instance.get_refresh_ms();
    }

    std::vector<dtc> obd2::get_dtcs(uint32_t ecu_id) {
        std::vector<dtc> dtcs;
        dtc::status statuses[] = { dtc::STORED, dtc::PENDING, dtc::PERMANENT };

        // TODO: Multiple requests at the same time
        for (dtc::status s : statuses) {
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, s, protocol_instance);

            if (c.wait_for_response() != cmd_status::OK) {
                continue;
            }

            const std::vector<uint8_t> &response = c.get_buffer();

            if (response.size() < 2) {
                continue;
            }

            std::vector<dtc> response_dtcs = decode_dtcs(response, s);
            dtcs.insert(dtcs.end(), response_dtcs.begin(), response_dtcs.end());
        }

        return dtcs;
    }

    void obd2::clear_dtcs(uint32_t ecu_id) {
        command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, 0x04, protocol_instance);
    }

    std::vector<dtc> obd2::decode_dtcs(const std::vector<uint8_t> &data, dtc::status status) {
        std::vector<dtc> dtcs;

        for (size_t i = 0; (i + 1) < data.size(); i += 2) {
            uint16_t raw_code = data[i] | data[i + 1] << 8;

            // Ignore zero bytes
            if (raw_code == 0) {
                continue;
            }

            dtcs.emplace_back(raw_code, status);
        }

        return dtcs;
    }
}
