#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "math_expr/math_expr.h"

namespace obd2 {
    class obd2;

    class request {
        private:
            obd2 *parent;

            uint32_t ecu_id;
            uint8_t service;
            uint16_t pid;

            std::string formula_str;
            math_expr formula;

            std::mutex value_mutex;
            std::vector<uint8_t> last_raw_value;
            float last_value = NO_RESPONSE;
            bool refresh = false;

            bool has_value() const;

        public:
            request(uint32_t ecu_id, uint8_t service, uint16_t pid, const std::string &formula, bool refresh, obd2 &parent);
            static const float NO_RESPONSE;
            
            void resume();
            void stop();

            float get_value();
            const std::vector<uint8_t> &get_raw();
            uint32_t get_ecu_id() const;
            uint8_t get_service() const;
            uint16_t get_pid() const;
            std::string get_formula() const;

            friend class obd2;
    };
}
