#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace obd2 {
    struct vehicle_info {
        enum ignition_type {
            SPARK = 0,
            COMPRESSION = 1,
            UNKNOWN = 2
        };

        struct ecu {
            uint32_t id;
            std::string name;
        };

        std::string vin;
        ignition_type ign_type;
        std::vector<ecu> ecus;
    };

    std::ostream &operator<<(std::ostream &os, const vehicle_info::ignition_type type);
}
