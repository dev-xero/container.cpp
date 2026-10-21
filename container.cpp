#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

constexpr size_t STACK_SIZE = 1024 * 1024;

[[noreturn]] void die(const char* message)
{
  perror(message);
  exit(EXIT_FAILURE);
}

[[noreturn]] void usage(const char* message)
{
  fprintf(stderr, "%s\nusage: ./container run <cmd> [args...]\n", message);
  exit(EXIT_FAILURE);
}

class Stack
{
public:
  explicit Stack(size_t size) : size_(size)
  {
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

  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  // Stacks grow downwards (as they should) on x86 and ARM
  // This returns the HIGH address i.e. the 'top' of the stack.
  void* top() const 
  {
    return static_cast<char*>(base_) + size_;
  }
  
private:
  void* base_;
  size_t size_;
};

class Container
{
public:
  Container(char** argv, std::string hostname = "container") 
    : argv_(argv), hostname_(std::move(hostname)) {}

  int run()
  {
    std::cout << "Running process " << argv_[0] << "\n";
    Stack stack(STACK_SIZE);
    pid_t pid = clone(
      &Container::child_entry,
      stack.top(),
      CLONE_NEWUSER | CLONE_NEWUTS | SIGCHLD, 
      this
    );

    if (pid == -1)
      die("clone failed");

    int status;
    if (waitpid(pid, &status, 0) == -1)
      die("waitpid failed");
    
    // See: https://man7.org/linux/man-pages/man2/wait.2.html
    return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
  }
  
private:
  static int child_entry(void* arg)
  {
    return static_cast<Container*>(arg) -> child_main();
  }
  
  int child_main()
  {
    if (sethostname(hostname_.c_str(), hostname_.length()) != 0)
    {
      perror("sethostname failed");
      _exit(1);
    }
    execvp(argv_[0], argv_);
    perror("execvp failed");
    _exit(1);
  }
  
  char** argv_;
  std::string hostname_;
};

int main(int argc, char* argv[])
{
  if (argc < 3)
    usage("invalid number of arguments");
  
  if (std::string(argv[1]).compare("run") != 0)
    usage("second parameter must be 'run'");
  
  Container container(&argv[2], "containerd");
  return container.run();
}
