#include "command.h"

#include <stdexcept>

#include "command_backend/command_backend.h"

namespace obd2 {
    command::command() : active_backend(nullptr) {}

    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, protocol &parent, bool refresh) 
        : command(tx_id, rx_id, sid, std::vector<uint16_t>(), parent, refresh) {}

    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, uint16_t pid, protocol &parent, bool refresh)
        : command(tx_id, rx_id, sid, std::vector<uint16_t>({ pid }), parent, refresh) {}

    command::command(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids, 
        protocol &parent, bool refresh) {
        active_backend = &find_backend(tx_id, rx_id, sid, pids, parent, refresh);
    }

    command::~command() {
        std::unique_lock<std::mutex> commands_lock(get_command_mutex());

        auto &command_usage = get_command_usage();

        command_usage[active_backend]--;

        if (command_usage[active_backend] == 0) {
            remove_backend(*active_backend);
        }

        active_backend = nullptr;
    }

    bool command::operator==(const command &c) const {
        return active_backend == c.active_backend;
    }

    void command::resume() {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        active_backend->resume();
    }

    void command::stop() {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        active_backend->stop();
    }

    uint32_t command::get_tx_id() const {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->get_tx_id();
    }

    uint32_t command::get_rx_id() const {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->get_rx_id();
    }

    uint8_t command::get_sid() const {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->get_sid();
    }

    const std::vector<uint16_t> &command::get_pids() {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->get_pids();
    }

    void command::set_pids(const std::vector<uint16_t> &pids) {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        active_backend->set_pids(pids);
    }

    void command::add_pid(uint16_t pid) {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        active_backend->add_pid(pid);
    }

    void command::remove_pid(uint16_t pid) {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        active_backend->remove_pid(pid);
    }

    bool command::contains_pid(uint16_t pid) {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->contains_pid(pid);
    }

    cmd_status command::get_response_status() {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->get_response_status();
    }

    cmd_status command::wait_for_response(uint32_t timeout_ms, uint32_t sample_us) {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->wait_for_response(timeout_ms, sample_us);
    }

    const std::vector<uint8_t> &command::get_buffer() {
        if (!active_backend) {
            throw std::runtime_error("Command not initialized");
        }

        return active_backend->get_buffer();
    }

    command_backend &command::find_backend(uint32_t tx_id, uint32_t rx_id, uint8_t sid, const std::vector<uint16_t> &pids, 
        protocol &parent, bool refresh) {
        std::unique_lock<std::mutex> commands_lock(get_command_mutex());

        // Check if identical command already exists
        for (command_backend &c : get_command_backends()) {
            if (c.tx_id != tx_id || c.rx_id != rx_id || c.sid != sid || c.parent != &parent) {
                continue;
            }
            
            if (c.get_pids() != pids) {
                continue;
            }

            return c;
        }

        command_backend &c = get_command_backends().emplace_back(tx_id, rx_id, sid, pids, parent, refresh);
        get_command_usage().emplace(&c, 1);
        
        return c;
    }

    void command::remove_backend(command_backend &c) {
        get_command_usage().erase(&c);

        auto &commands = get_command_backends();
        
        for (auto it = commands.begin(); it != commands.end(); it++) {
            if (&*it == &c) {
                commands.erase(it);
                break;
            }
        }
    }

    std::list<command_backend> &command::get_command_backends() {
        static std::list<command_backend> *commands = new std::list<command_backend>();

        return *commands;
    }

    std::unordered_map<command_backend *, std::atomic<uint32_t>> &command::get_command_usage() {
        static std::unordered_map<command_backend *, std::atomic<uint32_t>> *command_usage = new std::unordered_map<command_backend *, std::atomic<uint32_t>>();

        return *command_usage;
    }

    std::mutex &command::get_command_mutex() {
        static std::mutex *commands_mutex = new std::mutex();

        return *commands_mutex;
    }
}
