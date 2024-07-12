#pragma once

#include <cstdlib>
#include <cstdint>
#include <vector>

namespace obd2 {    
    class socket_wrapper {
        private:
            uint32_t tx_id;
            uint32_t rx_id;
            int fd;
        
        public:
            socket_wrapper(uint32_t tx_id, uint32_t rx_id, unsigned int if_index);
            ~socket_wrapper();
            socket_wrapper(socket_wrapper &&s);
            socket_wrapper(const socket_wrapper &s) = delete;
            socket_wrapper &operator=(const socket_wrapper &s) = delete;

            size_t read_msg(void *data, size_t size);
            void send_msg(void *data, size_t size);

            friend class protocol;
    };
}