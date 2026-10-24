#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sched.h>
#include <string>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr size_t STACK_SIZE = 1024 * 1024;

[[noreturn]] void die(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

[[noreturn]] void usage(const char *message) {
    fprintf(stderr, "%s\nusage: ./container run <cmd> [args...]\n", message);
    exit(EXIT_FAILURE);
}

class Stack {
  public:
    explicit Stack(size_t size) : size_(size) {
        // MAP_STACK flag is only recognized on Linux
        // This is fine because that is where this container should be running
        base_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
        if (base_ == MAP_FAILED)
            die("mmap failed");

        // See: https://man7.org/linux/man-pages/man3/sysconf.3.html
        const size_t page = sysconf(_SC_PAGESIZE);
        if (mprotect(base_, page, PROT_NONE) != 0)
            die("mprotect failed");
    }

    ~Stack() { munmap(base_, size_); }

    Stack(const Stack &) = delete;
    Stack &operator=(const Stack &) = delete;

    // Stacks grow downwards (as they should) on x86 and ARM
    // This returns the HIGH address i.e. the 'top' of the stack.
    void *top() const { return static_cast<char *>(base_) + size_; }

  private:
    void *base_;
    size_t size_;
};

class Container {
  public:
    Container(char **argv, std::string hostname = "container") : argv_(argv), hostname_(std::move(hostname)) {}

    int run() {
        std::cout << "Running process " << argv_[0] << " with PID " << getpid() << "\n";
        Stack stack(STACK_SIZE);

        if (pipe(sync_pipe_) == -1)
            die("pipe failed");

        // See: http://man7.org/linux/man-pages/man2/clone.2.html
        pid_t pid = clone(&Container::child_entry, stack.top(),
                          CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, this);

        if (pid == -1)
            die("clone failed");

        close(sync_pipe_[0]);

        // Permission errors arise otherwise
        // See: https://linux.die.net/man/2/setgroups
        //      https://linux.die.net/man/3/uid
        write_file("/proc/" + std::to_string(pid) + "/uid_map", "0 " + std::to_string(getuid()) + " 1\n");
        write_file("/proc/" + std::to_string(pid) + "/setgroups", "deny\n");
        write_file("/proc/" + std::to_string(pid) + "/gid_map", "0 " + std::to_string(getgid()) + " 1\n");

        char go = '1';
        if (write(sync_pipe_[1], &go, 1) != 1)
            die("pipe write failed (child)");
        close(sync_pipe_[1]);

        int status;
        if (waitpid(pid, &status, 0) == -1)
            die("waitpid failed");

        // See: https://man7.org/linux/man-pages/man2/wait.2.html
        return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
    }

  private:
    static int child_entry(void *arg) { return static_cast<Container *>(arg)->child_main(); }

    void write_file(const std::string &path, const std::string &value) {
        std::ofstream file(path);
        if (!file || !(file << value)) {
            die("write_file failed");
        }
    }

    int child_main() {
        close(sync_pipe_[1]);
        char go;
        ssize_t n = read(sync_pipe_[0], &go, 1);
        close(sync_pipe_[0]);

        if (n != 1)
            die("synchronization failed");

        // We don't want the container's mount relationships to propagate to
        // the VM, just like Liz did.
        // This recursively makes mount propagation private.
        if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == -1) {
            perror("failed to make mount private");
            _exit(1);
        }

        if (sethostname(hostname_.c_str(), hostname_.length()) != 0) {
            perror("sethostname failed");
            _exit(1);
        }

        // Filesystem root directory
        // TODO: make this passable via flags
        if (chroot("/opt/alpine-rootfs") == -1) {
            perror("chroot failed");
            _exit(1);
        }

        if (chdir("/") == -1) {
            perror("chdir failed");
            _exit(1);
        }

        // See: https://man7.org/linux/man-pages/man2/mount.2.html
        if (mount("proc", "/proc", "proc", 0, nullptr) == -1) {
            perror("mount failed");
            _exit(1);
        }

        char *const args[] = {const_cast<char *>("/bin/sh"), nullptr};

        std::cout << "arg0: " << argv_[0] << "\n";
        execvp(argv_[0], args);
        perror("execvp failed");

        if (umount("/proc") == -1)
            perror("unmount failed");

        _exit(1);
    }

    char **argv_;
    std::string hostname_;
    int sync_pipe_[2] = {-1, -1}; // read, write
};

int main(int argc, char *argv[]) {
    if (argc < 3)
        usage("invalid number of arguments");

    if (std::string(argv[1]).compare("run") != 0)
        usage("second parameter must be 'run'");

    Container container(&argv[2], "host-container");
    return container.run();
}
