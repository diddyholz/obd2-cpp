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
        }

        return os;
    }

    std::ostream &operator<<(std::ostream &os, const dtc &dtc) {
        switch (dtc.cat)
        {
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
        }

        os << std::setw(4) << std::setfill("0") << dtc.code 
            << std::setw(0) << " (" << dtc.stat << ")";

        return os;
    }
}
