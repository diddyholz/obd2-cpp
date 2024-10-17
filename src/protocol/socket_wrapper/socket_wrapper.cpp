#include "socket_wrapper.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/can/isotp.h>
#include <system_error>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#define UDS_PADDING_RX 0x00
#define UDS_PADDING_TX 0xCC

namespace obd2 {
    socket_wrapper::socket_wrapper(uint32_t tx_id, uint32_t rx_id, unsigned int if_index) 
        : tx_id(tx_id), rx_id(rx_id), fd(-1) {
        int s;

        can_isotp_options isotp_opt;
        sockaddr_can addr;

        if ((s = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)) < 0) {
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Setup ISO-TP options
        isotp_opt = {};
        isotp_opt.txpad_content = UDS_PADDING_TX;
        isotp_opt.rxpad_content = UDS_PADDING_RX;
        isotp_opt.flags = CAN_ISOTP_TX_PADDING | CAN_ISOTP_RX_PADDING;

        if (setsockopt(s, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &isotp_opt, sizeof(isotp_opt)) < 0) {
            close(s);
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Setup CAN adress
        addr = {};
        addr.can_family = AF_CAN;
        addr.can_ifindex = static_cast<int>(if_index);
        addr.can_addr.tp.tx_id = tx_id;
        addr.can_addr.tp.rx_id = rx_id;

        // Bind address to socket
        if (bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
            close(s);
            throw std::system_error(std::error_code(errno, std::generic_category()));
        }

        // Enable non blocking read
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);

        fd = s;
    }

    socket_wrapper::~socket_wrapper() {
        if (fd >= 0) {
            close(fd);
        }
    }

    socket_wrapper::socket_wrapper(socket_wrapper &&s) 
        : tx_id(s.tx_id), rx_id(s.rx_id), fd(s.fd) {
        if (this == &s) {
            return;
        }
        
        s.fd = -1;
        s.tx_id = 0;
        s.rx_id = 0;
    }

    socket_wrapper &socket_wrapper::operator=(socket_wrapper &&s) {
        if (this == &s) {
            return *this;
        }

        tx_id = s.tx_id;
        rx_id = s.rx_id;
        fd = s.fd;

        s.fd = -1;
        s.tx_id = 0;
        s.rx_id = 0;

        return *this;
    }

    size_t socket_wrapper::read_msg(void *data, size_t size) {
        ssize_t nbytes = read(fd, data, size);

        // An error occured => Do not return anything
        if (nbytes <= 0) {
            return 0;
        }

        return static_cast<size_t>(nbytes);
    }
 
    void socket_wrapper::send_msg(void *data, size_t size) {
        while(write(fd, data, size) < 0) {
            if (errno != EAGAIN) {
                return;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}
