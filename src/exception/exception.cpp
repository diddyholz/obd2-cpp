#include "exceptions.h"

obd2::exception::exception(const char *msg) : message(msg) {

}

const char *obd2::exception::what() {
    return message;
}