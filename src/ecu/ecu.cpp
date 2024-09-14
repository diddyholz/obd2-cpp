#include "ecu.h"

namespace obd2 {
    ecu::ecu() : id(0) { }

    ecu::ecu(uint32_t id, std::string name) : id(id), name(name) { }

    void ecu::add_supported_pids(uint8_t service, const std::vector<uint8_t> &pids) {
        supported_pids[service] = pids;
    }

    uint32_t ecu::get_id() const {
        return id;
    }

    const std::string &ecu::get_name() const {
        return name;
    }

    const std::vector<uint8_t> &ecu::get_supported_pids(uint8_t service) const {
        auto it = supported_pids.find(service);

        if (it == supported_pids.end()) {
            static const std::vector<uint8_t> empty;
            return empty;
        }

        return it->second;
    }
}
