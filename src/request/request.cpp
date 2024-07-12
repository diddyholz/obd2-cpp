#include "request.h"

#include "../../include/obd2.h"

namespace obd2 {
    const float request::NO_RESPONSE = std::numeric_limits<float>::quiet_NaN();

    request::request(uint32_t ecu_id, uint8_t service, uint16_t pid, const std::string &formula, bool refresh, obd2 &parent)
        : parent(&parent), ecu_id(ecu_id), service(service), pid(pid), formula_str(formula), formula(formula), refresh(refresh) { }

    void request::resume() {
        parent->resume_request(*this);
    }

    void request::stop() {
        parent->stop_request(*this);
    }

    float request::get_value() {
        std::lock_guard<std::mutex> value_lock(value_mutex);

        if (refresh || !has_value()) {
            last_raw_value = parent->get_data(*this);
        }

        if (last_raw_value.size() == 0) {
            last_value = NO_RESPONSE;
            return last_value;
        }

        last_value = formula.solve(last_raw_value);
        return last_value; 
    }

    const std::vector<uint8_t> &request::get_raw() {
        std::lock_guard<std::mutex> value_lock(value_mutex);

        if (refresh || !has_value()) {
            last_raw_value = parent->get_data(*this);
        }

        return last_raw_value;
    }

    uint32_t request::get_ecu_id() const {
        return ecu_id;    
    }

    uint8_t request::get_service() const {
        return service;    
    }

    uint16_t request::get_pid() const {
        return pid;    
    }

    std::string request::get_formula() const {
        return formula_str;    
    }

    bool request::has_value() const {
        return last_raw_value.size() > 0;
    }
}
