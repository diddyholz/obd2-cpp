#include "socket_wrapper.h"

#include <cerrno>
#include <cstring>

#include "../exception/exception.h"
#include "../macros.h"

extern "C" {
    #include <fcntl.h>
    #include <sys/socket.h>
    #include <unistd.h>
    // #include <sys/ifctl.h>
    #include <linux/can/isotp.h>
}

namespace obd2 {
    socket_wrapper::socket_wrapper(uint32_t tx_id, uint32_t rx_id, int if_index) 
        : tx_id(tx_id), rx_id(rx_id), fd(-1) {
        int s;

        can_isotp_options isotp_opt;
        sockaddr_can addr;

        if ((s = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)) < 0) {
            throw socket_exception((const char *)std::strerror(errno));
        }

        // Setup ISO-TP options
        CLEAR_STRUCT(isotp_opt);
        isotp_opt.txpad_content = 0xCC;
        isotp_opt.rxpad_content = 0x00;

        setsockopt(s, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &isotp_opt, sizeof(isotp_opt));

        // Setup CAN adress
        CLEAR_STRUCT(addr);
        addr.can_family = AF_CAN;
        addr.can_ifindex = if_index;
        addr.can_addr.tp.tx_id = tx_id;
        addr.can_addr.tp.rx_id = rx_id;

        // Bind address to socket
        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            throw socket_exception((const char *)std::strerror(errno));
        }

        // Enable non blocking read
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);

        this->fd = s;
    }

    socket_wrapper::~socket_wrapper() {
        close(this->fd);
    }

    socket_wrapper::socket_wrapper(socket_wrapper &&s) 
        : fd(s.fd), tx_id(s.tx_id), rx_id(s.rx_id) {
        s.fd = -1;
        s.tx_id = 0;
        s.rx_id = 0;
    }

    size_t socket_wrapper::read_msg(void *data, size_t size) {
        ssize_t nbytes = read(this->fd, data, size);

        // An error occured => Do not return anything
        if (nbytes <= 0) {
            return 0;
        }

        return (size_t)nbytes;
    }
}
