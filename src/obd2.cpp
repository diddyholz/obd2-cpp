#include "../include/obd2.h"

#include <stdexcept>

#define OBD2_ID_BROADCAST   0x7DF
#define OBD2_ID_FIRST       0x7E0
#define OBD2_ID_LAST        0x7E7

namespace obd2 {
    obd2::obd2(const char *if_name, uint32_t refresh_ms) 
        : protocol_instance(if_name, refresh_ms) {}

    obd2::~obd2() {
        // Cleanup all requests
        for (auto &p : requests_command_map) {
            delete p.first;
        }
    }

    request &obd2::add_request(uint32_t ecu_id, uint8_t service, uint16_t pid, const std::string &formula, bool refresh) {
        // TODO: Support broadcast id (0x7DF)
        if (ecu_id < OBD2_ID_FIRST || ecu_id > OBD2_ID_LAST) {
            throw std::invalid_argument("Invalid or unsupported ECU ID");
        }

        command &c = protocol_instance.add_command(ecu_id, ecu_id + 8, service, pid, refresh);
    	
        // Check if request already exists
        for (auto &p : requests_command_map) {
            request *r = p.first;
            
            if (r->ecu_id == ecu_id && r->service == service && r->pid == pid && r->formula_str == formula) {
                throw std::invalid_argument("A request with the specified parameters already exists");
            }
        }
        
        request *new_request = new request(ecu_id, service, pid, formula, refresh, *this);
        requests_command_map.emplace(new_request, c);

        return *new_request;
    }

    void obd2::set_refresh_ms(uint32_t refresh_ms) {
        protocol_instance.set_refresh_ms(refresh_ms);
    }
    
    // TODO: For faster counting, use a separate map<command *, size_t> instead of iterating over all requests
    size_t obd2::requests_using_command(const command &c) const {
        size_t count = 0;

        for (auto &p : requests_command_map) {
            if (&p.second.get() != &c) {
                continue;
            }

            if (p.first->refresh || !p.first->has_value()) {
                count++;
            }
        }

        return count;
    }

    void obd2::stop_request(request &r) {
        if (!r.refresh) {
            return;
        }
    
        r.refresh = false;

        command &c = requests_command_map.at(&r);

        if (requests_using_command(c) == 0) {
            c.stop();
        }
    }

    void obd2::resume_request(request &r) {
        if (r.refresh) {
            return;
        }

        r.refresh = true;
        requests_command_map.at(&r).get().resume();
    }

    const std::vector<uint8_t> &obd2::get_data(const request &r) {
        return requests_command_map.at(const_cast<request *>((&r))).get().get_current_buf();
    }
}
