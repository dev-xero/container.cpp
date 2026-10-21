#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <sched.h>
#include <sys/mman.h>

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
    {
      die("mmap failed");
    }

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
  
private:
};

int child(void* arg)
{
  const char* proc = static_cast<char*>(arg);
  if (sethostname("container", 9) != 0)
  {
    perror("sethostname failed");
    return 1;
  }
  char* args[] = { const_cast<char*>(proc), NULL };
  execvp(args[0], args);
  perror("execvp failed");
  return 1;
}

void start_process(char** argv)
{
}

int main(int argc, char* argv[])
{
  if (argc < 3)
    usage("invalid number of arguments");
  
  if (std::string(argv[1]).compare("run") != 0)
    usage("second parameter must be 'run'");
  
  return start_process(argv + 2);
}
