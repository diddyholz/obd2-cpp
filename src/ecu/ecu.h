#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace obd2 {
    class ecu {
        public:
            ecu();
            ecu(uint32_t id, std::string name);
            
            void add_supported_pids(uint8_t service, const std::vector<uint8_t> &pids);

            uint32_t get_id() const;
            const std::string &get_name() const;
            const std::vector<uint8_t> &get_supported_pids(uint8_t service) const;

        private:
            uint32_t id;
            std::string name;
            std::unordered_map<uint8_t, std::vector<uint8_t>> supported_pids; // Service => PIDs
    };
}
