#include "dtc.h"

#include <iomanip>
#include <sstream>

namespace obd2 {
    dtc::dtc() : cat(category(0xFF)), code(0), stat(status(0xFF)) {}

    dtc::dtc(uint16_t raw_code, status stat) : stat(stat) {
        code = ((raw_code & 0x003F) << 8) | ((raw_code & 0xFF00) >> 8);
        cat = category((raw_code & 0xC0) >> 6);
    }

    dtc::category dtc::get_category() const {
        return cat;
    }

    uint16_t dtc::get_code() const {
        return code;
    }

    dtc::status dtc::get_status() const {
        return stat;
    }

    std::ostream &operator<<(std::ostream &os, const dtc::status stat) {
        switch (stat) {
            case dtc::status::STORED:
                os << "Stored";
                break;
            case dtc::status::PENDING:
                os << "Pending";
                break;
            case dtc::status::PERMANENT:
                os << "Permanent";
                break;
            default:
                os << "Unknown";
        }

        return os;
    }

    std::ostream &operator<<(std::ostream &os, const dtc &dtc) {
        std::string code = dtc.str();

        os << code << " (" << dtc.stat << ")";

        return os;
    }

    std::string dtc::str() const {
        std::stringstream ss;

        switch (cat) {
            case dtc::category::POWERTRAIN:
                ss << "P";
                break;
            case dtc::category::CHASSIS:
                ss << "C";
                break;
            case dtc::category::BODY:
                ss << "B";
                break;
            case dtc::category::NETWORK:
                ss << "U";
                break;
            default:
                ss << "X";
                break;
        }

        // Print nibbles of the code
        for (int i = 0; i < 4; i++) {
            ss << std::hex << std::uppercase << ((code >> (12 - i * 4)) & 0xF);
        }

        return ss.str();
    }
}
