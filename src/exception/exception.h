#pragma once

#include <exception>

namespace obd2 {
    class exception : public std::exception {
        private:
            const char *message;

        public:
            exception(const char *msg);
            
            const char *what();
    };
}