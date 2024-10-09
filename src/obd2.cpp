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

    std::vector<dtc> obd2::get_dtcs(uint32_t ecu_id) {
        std::vector<dtc> dtcs;
        dtc::status statuses[] = { dtc::STORED, dtc::PENDING, dtc::PERMANENT };

        for (dtc::status s : statuses) {
            command c(ecu_id, ecu_id + ECU_ID_RES_OFFSET, s, 0, protocol_instance);

            if (c.wait_for_response() != command::OK) {
                continue;
            }

            if (c.get_buffer().size() < 2) {
                continue;
            }

            std::vector<uint8_t> data(c.get_buffer().begin() + 1, c.get_buffer().end());
            std::vector<dtc> response_dtcs = decode_dtcs(data, s);
            dtcs.insert(dtcs.end(), response_dtcs.begin(), response_dtcs.end());
        }

        return dtcs;
    }

    std::vector<dtc> obd2::decode_dtcs(const std::vector<uint8_t> &data, dtc::status status) {
        std::vector<dtc> dtcs;

        for (size_t i = 0; i < data.size(); i += 2) {
            uint16_t raw_code = data[i] | data[i + 1];

            dtcs.emplace_back(raw_code, status);
        }

        return dtcs;
    }
}
