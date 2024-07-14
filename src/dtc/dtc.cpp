#include "dtc.h"

#include <iomanip>

namespace obd2 {
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
        switch (dtc.cat) {
            case dtc::category::POWERTRAIN:
                os << "P";
                break;
            case dtc::category::CHASSIS:
                os << "C";
                break;
            case dtc::category::BODY:
                os << "B";
                break;
            case dtc::category::NETWORK:
                os << "U";
                break;
            default:
                os << "x";
                break;
        }

        char old_fill = os.fill('0');
        std::streamsize old_width = os.width(4);

        os << dtc.code << " (" << dtc.stat << ")";

        os.fill(old_fill);
        os.width(old_width);

        return os;
    }
}
