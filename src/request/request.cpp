#include "request.h"

#include "../../include/obd2.h"

namespace obd2 {
    request::request() : parent(nullptr) { }

    request::request(uint32_t ecu_id, uint8_t service, uint16_t pid, obd2 &parent, const std::string &formula, bool refresh)
        : parent(&parent), ecu_id(ecu_id), service(service), pid(pid), formula_str(formula), formula(formula), refresh(refresh) { 
        // TODO: Support broadcast id (0x7DF)
        if (ecu_id < obd2::ECU_ID_FIRST || ecu_id > obd2::ECU_ID_LAST) {
            throw std::invalid_argument("Invalid or unsupported ECU ID");
        }

        parent.add_request(*this);
    }

    request::request(request &&r) {
        if (this == &r) {
            return;
        }

        if (parent != nullptr) {
            parent->remove_request(*this);
        }

        parent = r.parent;
        ecu_id = r.ecu_id;
        service = r.service;
        pid = r.pid;
        formula_str = r.formula_str;
        formula = r.formula;
        refresh = r.refresh;
        last_value = r.last_value;

        r.parent = nullptr;

        if (parent != nullptr) {
            parent->move_request(r, *this);
        }

        last_raw_value = std::move(r.last_raw_value);
    }

    request::~request() {
        if (parent != nullptr) {
            parent->remove_request(*this);
        }
    }

    request &request::operator=(request &&r) {
        if (this == &r) {
            return *this;
        }

        if (parent != nullptr) {
            parent->remove_request(*this);
        }

        parent = r.parent;
        ecu_id = r.ecu_id;
        service = r.service;
        pid = r.pid;
        formula_str = r.formula_str;
        formula = r.formula;
        refresh = r.refresh;
        last_value = r.last_value;

        r.parent = nullptr;

        if (parent != nullptr) {
            parent->move_request(r, *this);
        }

        last_raw_value = std::move(r.last_raw_value);

        return *this;
    }

    void request::resume() {
        check_parent();

        parent->resume_request(*this);
    }

    void request::stop() {
        check_parent();

        parent->stop_request(*this);
    }

    float request::get_value() {
        check_parent();

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
        check_parent();
        
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

    void request::check_parent() {
        if (parent == nullptr) {
            throw std::runtime_error("Request has no parent");
        }
    }

    bool request::has_value() const {
        return last_raw_value.size() > 0;
    }
}
