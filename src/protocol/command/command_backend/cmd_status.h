#pragma once

namespace obd2 {
    enum cmd_status {
        OK = 0,
        WAITING,
        NO_RESPONSE,
        ERROR
    };
}
