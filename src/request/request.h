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
            static constexpr float NO_RESPONSE = std::numeric_limits<float>::quiet_NaN();

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

            void check_parent();
            bool has_value() const;

        public:
            request();
            request(uint32_t ecu_id, uint8_t service, uint16_t pid, obd2 &parent, const std::string &formula = "", bool refresh = false);
            request(const request &r) = delete;
            request(request &&r);
            ~request();

            request &operator=(const request &r) = delete;
            request &operator=(request &&r);
            bool operator==(const request &r) const;
            
            void resume();
            void stop();

            float get_value();
            const std::vector<uint8_t> &get_raw();
            uint32_t get_ecu_id() const;
            uint8_t get_service() const;
            uint16_t get_pid() const;
            std::string get_formula() const;
            size_t get_expected_size();
            bool get_refresh() const;

            friend class obd2;
    };
}
