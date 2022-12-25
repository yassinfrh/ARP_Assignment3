#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>

#define SEM_PATH "/sem_SHARED_IMAGE"

int spawn(const char *program, char *arg_list[])
{

  pid_t child_pid = fork();

  if (child_pid < 0)
  {
    perror("Error while forking...");
    return 1;
  }

  else if (child_pid != 0)
  {
    return child_pid;
  }

  else
  {
    if (execvp(program, arg_list) == 0)
      ;
    perror("Exec failed");
    return 1;
  }
}

int main()
{

  // Create a semaphore
  sem_t *sem = sem_open(SEM_PATH, O_CREAT, S_IRUSR | S_IWUSR, 1);
  if (sem == SEM_FAILED)
  {
    perror("Error while creating semaphore");
    return 1;
  }

  // Initialize the semaphore
  if (sem_init(sem, 1, 1) == -1)
  {
    perror("Error while initializing semaphore");
    return 1;
  }

  // Close the semaphore
  if (sem_close(sem) == -1)
  {
    perror("Error while closing semaphore");
    return 1;
  }

  char *arg_list_A[] = {"/usr/bin/konsole", "-e", "./bin/processA", NULL};
  char *arg_list_B[] = {"/usr/bin/konsole", "-e", "./bin/processB", NULL};

  pid_t pid_procA = spawn("/usr/bin/konsole", arg_list_A);
  if (pid_procA == 1)
  {
    return 1;
  }

  pid_t pid_procB = spawn("/usr/bin/konsole", arg_list_B);
  if (pid_procB == 1)
  {
    // Kill the other process
    kill(pid_procA, SIGKILL);
    return 1;
  }

  int status;

  while (1)
  {
    // If child process terminates unexpectedly, kill the other child process
    if (waitpid(pid_procA, &status, WNOHANG) != 0)
    {
      kill(pid_procB, SIGKILL);
      break;
    }
    else if (waitpid(pid_procB, &status, WNOHANG) != 0)
    {
      kill(pid_procA, SIGKILL);
      break;
    }
  }

  // Check the exit status of the child process
  if (WEXITSTATUS(status) == 1)
  {
    // If the status is 1, the child process exited because of a bitmap error
    printf("Child process exited for bitmap error\n");
    fflush(stdout);
  }
  else if (WEXITSTATUS(status) > 1)
  {
    // If the status is greater than 1, the child process exited because of a system call error
    printf("Child process exited for system call error: %s", strerror(WEXITSTATUS(status)));
  }

  // Unlink the semaphore
  if (sem_unlink(SEM_PATH) == -1)
  {
    perror("Error while unlinking semaphore");
    return 1;
  }

  return 0;
}