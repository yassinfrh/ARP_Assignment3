#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>

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

  // Variable to store the user's choice
  char choice[20];
  int modality;

  while (1)
  {
    do
    {
      /// Ask the user if they which modality to launch the program
      printf("Which modality do you want to launch the program in (insert the number)?\n");
      printf("1. Normal\n");
      printf("2. Server\n");
      printf("3. Client\n");
      printf("4. Exit\n");

      // Read the user's choice as a string
      scanf("%s", choice);

      // Convert the user's choice to an integer
      modality = atoi(choice);

      // Check if the user's choice is valid
      if (modality == 1 || modality == 2 || modality == 3 || modality == 4)
      {
        break;
      }
      else
      {
        printf("Invalid choice. Please try again.\n");
      }
    } while (1);

    // Store the modality in a string to pass it to the child process
    char modality_str[2];
    sprintf(modality_str, "%d", modality);

    // Variable to store the port number
    char port_str[6];
    // Variable to store the IP address
    char ip_str[16];

    // If the user chose to launch the program in server mode, ask the user to insert the port number
    if (modality == 2)
    {
      char port[20];
      do
      {
        printf("Insert the port number (between 1024 and 65535): ");
        scanf("%s", port);

        // Check if the port number is valid
        if (atoi(port) >= 1024 && atoi(port) <= 65535)
        {
          break;
        }
        else
        {
          printf("Invalid port number. Please try again.\n");
        }
      } while (1);

      // Store the port number in a string to pass it to the child process
      sprintf(port_str, "%d", atoi(port));
    }
    // If the user chose to launch the program in client mode, ask the user to insert the IP and the port
    else if (modality == 3)
    {
      char ip[16];
      do
      {
        printf("Insert the IP address: ");
        scanf("%s", ip);

        // Check if the IP address is valid
        if (strcmp(ip, "localhost") == 0 || inet_addr(ip) != -1)
        {
          break;
        }
        else
        {
          printf("Invalid IP address. Please try again.\n");
        }
      } while (1);

      // Store the IP address in a string to pass it to the child process
      sprintf(ip_str, "%s", ip);

      char port[20];
      do
      {
        printf("Insert the port number (between 1024 and 65535): ");
        scanf("%s", port);

        // Check if the port number is valid
        if (atoi(port) >= 1024 && atoi(port) <= 65535)
        {
          break;
        }
        else
        {
          printf("Invalid port number. Please try again.\n");
        }
      } while (1);

      // Store the port number in a string to pass it to the child process
      sprintf(port_str, "%d", atoi(port));
    }

    // If the user chose to exit the program, exit the loop
    else if (modality == 4)
    {
      break;
    }

    // Create the argument list for the child processes
    char *arg_list_A[] = {"/usr/bin/konsole", "-e", "./bin/processA", modality_str, port_str, ip_str, NULL};
    char *arg_list_B[] = {"/usr/bin/konsole", "-e", "./bin/processB", NULL};

    pid_t pid_procA = spawn("/usr/bin/konsole", arg_list_A);
    if (pid_procA == 1)
    {
      return 1;
    }

    usleep(500000);

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

    printf("Program exited.\n");
    fflush(stdout);
  }

  // Unlink the semaphore
  if (sem_unlink(SEM_PATH) == -1)
  {
    perror("Error while unlinking semaphore");
    return 1;
  }

  return 0;
}