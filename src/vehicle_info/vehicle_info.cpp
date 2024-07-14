#include "vehicle_info.h"

namespace obd2 {
    std::ostream &operator<<(std::ostream &os, const vehicle_info::ignition_type type) {
        switch (type) {
            case vehicle_info::ignition_type::SPARK:
                os << "Spark";
                break;
            case vehicle_info::ignition_type::COMPRESSION:
                os << "Compression";
                break;
            case vehicle_info::ignition_type::UNKNOWN:
                os << "Unknown";
                break;
        }

        return os;
    }
}