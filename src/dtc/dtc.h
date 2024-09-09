#pragma once

#include <cstdint>
#include <iostream>

namespace obd2 {
    class dtc {
        public:
            // Enum value equals corresponding service
            enum status {
                STORED = 0x03,
                PENDING = 0x07,
                PERMANENT = 0x0A
            };

            enum category {
                POWERTRAIN = 0,
                CHASSIS = 1,
                BODY = 2,
                NETWORK = 3
            };

            dtc();
            dtc(uint16_t raw_code, status stat);

            category get_category() const;
            uint16_t get_code() const;
            status get_status() const;
        
        private:
            category cat;
            uint16_t code;
            status stat;

            friend std::ostream &operator<<(std::ostream &os, const dtc &dtc);
    };

    std::ostream &operator<<(std::ostream &os, const dtc::status stat);
    std::ostream &operator<<(std::ostream &os, const dtc &dtc);
}