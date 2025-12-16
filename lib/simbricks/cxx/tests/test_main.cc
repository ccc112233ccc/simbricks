#include <iostream>
#include <string>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>

#include "base/proto.h"
#include "cxx/base/endpoint.h"
#include "cxx/communicator/shm_communicator.h"

// A simple message structure for our test.
// IMPORTANT: It must be padded to the size of a cache line (64 bytes)
// to match the underlying protocol's assumptions.
struct alignas(64) TestMessage {
  // Embed the base header directly to ensure correct layout.
  SimbricksProtoBaseMsgHeader header;

  // Custom payload
  uint64_t val;
  uint64_t seq;
};

void run_listener(const std::string& sock_path, const std::string& shm_path) {
    auto comm = std::make_unique<simbricks::communicator::ShmCommunicator>(
        sock_path, shm_path, true);
    simbricks::base::Endpoint listener(std::move(comm));

    if (listener.Listen() != 0) {
        std::cerr << "Listener failed to listen" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Wait for connector to establish connection
    int wait_cycles = 0;
    while (!listener.PollConnection()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        wait_cycles++;
        if (wait_cycles > 200) { // 2 seconds timeout
            std::cerr << "Listener timeout waiting for connection" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    std::cout << "Listener connected" << std::endl;

    bool received = false;
    for (int i = 0; i < 200; ++i) { // Poll for up to 2 seconds
        received = listener.Receive<TestMessage>(0, [&](volatile TestMessage* msg) {
            std::cout << "Listener received message with val: " << msg->val << " seq: " << msg->seq << std::endl;
            if (msg->val != 12345 || msg->seq != 1) {
                std::cerr << "Listener received incorrect value" << std::endl;
                exit(EXIT_FAILURE);
            }
        });
        if (received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!received) {
        std::cerr << "Listener failed to receive message" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Listener test passed" << std::endl;
}

void run_connector(const std::string& sock_path, const std::string& shm_path) {
    // Give listener time to start up and create the socket file
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto comm = std::make_unique<simbricks::communicator::ShmCommunicator>(
        sock_path, shm_path, false);
    simbricks::base::Endpoint connector(std::move(comm));

    if (connector.Connect() != 0) {
        // This might fail if the listener is not ready, so we can retry
    }

    int wait_cycles = 0;
    while (!connector.PollConnection()) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         wait_cycles++;
         if (wait_cycles > 200) {
             std::cerr << "Connector timeout waiting for connection" << std::endl;
             exit(EXIT_FAILURE);
         }
    }
    std::cout << "Connector connected" << std::endl;

    bool sent = connector.Send<TestMessage>(0, SIMBRICKS_PROTO_MSG_TYPE_UPPER_START, [](volatile TestMessage* msg) {
        msg->val = 12345;
        msg->seq = 1;
        std::cout << "Connector sending message with val: " << msg->val << " seq: " << msg->seq << std::endl;
    });

    if (!sent) {
        std::cerr << "Connector failed to send message" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Connector test passed" << std::endl;
}

int main(int argc, char *argv[]) {
    // Generate unique paths for parallel test execution
    std::string sock_path = "/tmp/simbricks-test." + std::to_string(getpid()) + ".sock";
    std::string shm_path = "/simbricks-test." + std::to_string(getpid());

    // Clean up previous runs just in case
    unlink(sock_path.c_str());
    shm_unlink(shm_path.c_str());

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Child process
        run_connector(sock_path, shm_path);
        // exit() is important here to not continue to parent's code
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        run_listener(sock_path, shm_path);

        int status;
        waitpid(pid, &status, 0);

        // Clean up
        unlink(sock_path.c_str());
        shm_unlink(shm_path.c_str());

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            std::cout << "End-to-end test PASSED" << std::endl;
            return EXIT_SUCCESS;
        } else {
            std::cerr << "End-to-end test FAILED with status " << WEXITSTATUS(status) << std::endl;
            return EXIT_FAILURE;
        }
    }
}
