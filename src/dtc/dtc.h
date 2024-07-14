#pragma once

#include <cstdint>

namespace obd2 {
    struct dtc {
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

        category cat;
        uint16_t code;
        status stat;
    };
}