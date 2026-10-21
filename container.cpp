#include <iostream>
#include <unistd.h>

void start_process(std::string);
void terminate(std::string);

// Spawn a process.
void start_process(std::string proc_name)
{
  std::cout << "Starting process " << proc_name << "\n";
  pid_t child_pid = fork();
  if (child_pid == -1)
  {
    terminate("Failed to start child process");
  }
  if (child_pid > 0)
  {
    std::cout << "Hello from parent! pid: " << getpid() << "\n";
  }
  else 
  {
    std::cout << "Hello from child, pid: " << getpid() << "\n";
  }
}

// Terminate the program after writing the error message to stderr.
void terminate(std::string message)
{
  std::cerr << message << '\n';
  std::cerr << "Exiting...\n";
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    terminate("Invalid number of arguments, usage: ./container run <cmd>");
  }
  if (std::string("run").compare(argv[1]) != 0)
  {
    terminate("Second parameter must be 'run', usage: ./container run <cmd>");
  }
  start_process(argv[2]);
  return 0;
}
