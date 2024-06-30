#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <thread>
#include <mutex>

#include "../src/request/request.h"
#include "../src/socket_wrapper/socket_wrapper.h"

namespace obd2 {
    class instance {
        private:
            // Pointer to requests to make sure that references do not get invalidated
            std::list<request *> active_requests;
            std::list<request *> prev_requests;
            std::mutex requests_mutex;

            std::vector<socket_wrapper> sockets;

            int if_index;
            uint32_t refresh_ms;
            std::thread listener_thread;

            void response_listener();
            void open_default_sockets();
            void remove_completed_requests();
            void process_request(request &r);
            void process_socket(socket_wrapper &s);

        public:
            instance(const char *if_name, uint32_t refresh_ms = 1000);
            instance(const instance &i) = delete;
            instance(const instance &&i) = delete;
            ~instance();            
            
            instance &operator=(const instance &i) = delete;

            void set_refresh_ms(uint32_t refresh_ms);
            request &add_request(uint32_t tx_id, uint8_t sid, uint16_t pid, bool refresh);
    };
}