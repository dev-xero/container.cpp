#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

[[noreturn]] void kill_child(const char *message) {
    perror(message);
    _exit(EXIT_FAILURE);
}

[[noreturn]] void usage(const char *message) {
    fprintf(stderr, "%s\nusage: ./container -fs <filesystem-root> [flags...] run <cmd> [args...]\n", message);
    fprintf(stderr, "\nflags:\n\t-mem, amount of memory in megabytes, default is 4\n\t-cpu, number of cpu cores "
                    "allowed, default is 1\n\n");

    exit(EXIT_FAILURE);
}

struct RunArguments {
    char *fileSystemRoot;
    char *subcommand;
    int subArgsOffset;
    int memInBytes;
    int cpuCores;
};

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
    Container(char **argv, const char *filesysroot, const char *subcommand, const int mem, const int cpu,
              const char *hostname = "container")
        : argv_(argv), filesysroot_(filesysroot), subcommand_(subcommand), mem_(mem), cpu_(cpu),
          hostname_(std::move(hostname)) {}

    int run() {
        std::clog << "\nSTARTING CONTAINER:\n";
        std::clog << "Running process " << subcommand_ << " with PID " << getpid() << "\n";

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
            kill_child("synchronization failed");

        // We don't want the container's mount relationships to propagate to
        // the VM. This recursively makes mount propagation private.
        if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == -1)
            kill_child("failed to make mount private");

        if (sethostname(hostname_, std::strlen(hostname_)) != 0)
            kill_child("sethostname failed");

        if (chroot(filesysroot_) == -1)
            kill_child("chroot failed");

        if (chdir("/") == -1)
            kill_child("chdir failed");

        if (mount("proc", "/proc", "proc", 0, nullptr) == -1)
            kill_child("mount failed");

        char *const args[] = {const_cast<char *>(subcommand_), const_cast<char *>(argv_[0]), nullptr};
        execvp(subcommand_, args);
        perror("execvp failed");

        if (umount("/proc") == -1)
            perror("unmount failed");

        _exit(1);
    }

    char **argv_;
    const char *filesysroot_;
    const char *hostname_;
    const char *subcommand_;
    const int mem_;
    const int cpu_;
    int sync_pipe_[2] = {-1, -1};
};

RunArguments parseArguments(int argc, char *argv[]) {
    char *fs, *subcommand = nullptr;
    int subArgsOffset = 0, mem = 0, cpu = 0;
    for (int i = 1; i < argc; i++) {
        if (i + 1 < argc) {
            if (strcmp(argv[i], "-fs") == 0) {
                fs = argv[i + 1];
                i++;
            }
            if (strcmp(argv[i], "-mem") == 0) {
                mem = atoi(argv[i + 1]) * 1024 * 1024;
                i++;
            }
            if (strcmp(argv[i], "-cpu") == 0) {
                cpu = atoi(argv[i + 1]);
                i++;
            }
            if (strcmp(argv[i], "run") == 0) {
                subcommand = argv[i + 1];
                subArgsOffset = i + 2;
                i++;
            }
        }
    }
    struct RunArguments args{.fileSystemRoot = fs,
                             .subcommand = subcommand,
                             .subArgsOffset = subArgsOffset,
                             .memInBytes = mem == 0 ? 4 * 1024 * 1024 : mem,
                             // TODO: this will pose a problem later, fractional cpus
                             .cpuCores = cpu == 0 ? 1 : cpu};
    return args;
}

int main(int argc, char *argv[]) {
    struct RunArguments args = parseArguments(argc, argv);

    if (args.fileSystemRoot == nullptr)
        usage("filesystem root flag must be specified");

    if (access(args.fileSystemRoot, F_OK) != 0)
        usage("filesystem root not specified or does not exist");

    if (args.subcommand == nullptr)
        usage("'run' command must be specified");

    std::clog << "\n\nDEBUG:\n";
    std::clog << "File system root: " << args.fileSystemRoot << "\n";
    std::clog << "Memory in bytes: " << args.memInBytes << "\n";
    std::clog << "CPU cores: " << args.cpuCores << "\n";
    std::clog << "Subcommand: " << args.subcommand << "\n";
    std::clog << "Subcommand args offset: " << args.subArgsOffset << "\n\n";

    Container container(&argv[args.subArgsOffset], args.fileSystemRoot, args.subcommand, args.memInBytes,
                        args.memInBytes, "host-container");
    return container.run();
}
