#pragma once

#include "../request/request.h"
#include "../protocol/protocol.h"

namespace obd2 {
    class req_combination {
        private:
            command cmd;
            std::list<std::reference_wrapper<request>> requests;
            bool allow_pid_chain;

        public:
            req_combination();
            req_combination(uint32_t ecu_id, uint8_t sid, uint16_t pid, protocol &protocol_instance, bool refresh, bool allow_pid_chain = true);
            req_combination(const req_combination &c) = delete;
            req_combination(req_combination &&c);

            req_combination &operator=(const req_combination &c) = delete;
            req_combination &operator=(req_combination &&c);

            bool operator==(const req_combination &c) const;

            void add_request(request &r);
            bool remove_request(request &r);
            void move_request(request &old_ref, request &new_ref);
            void request_stopped();
            void request_resumed();
            
            size_t get_pid_count();
            size_t get_var_count(uint16_t pid);
            bool contains_pid(uint16_t pid);
            bool get_allow_pid_chain() const;
            command &get_command();
    };
}