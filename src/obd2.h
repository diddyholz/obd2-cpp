#pragma once

#include <vector>
#include <list>
#include <cstdint>
#include <thread>

#include "socket_wrapper/socket_wrapper.h"
#include "request/request.h"

namespace obd2 {    
    class instance {
        private:
            std::vector<socket_wrapper> sockets;
            std::list<request *> active_requests;
            std::list<request *> prev_requests;

            std::thread listener_thread;

            int if_index;

            void open_default_sockets();
            void response_listener();

        public:
            instance(const char *if_name);
            ~instance();

            instance(const instance &i) = delete;
            instance &operator=(const instance &i) = delete;

            request &add_request(uint32_t tx_id, uint8_t sid, uint16_t pid, bool refresh);
    };
}
