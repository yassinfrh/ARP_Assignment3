#include "./../include/processA_utilities.h"
#include <bmpfile.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#define SEM_PATH "/sem_SHARED_IMAGE"

// Dimensions of the image
const int width = 1600;
const int height = 600;
const int depth = 4;

// Log file
FILE *logFile;

// Function to draw a circle of radius 30 on a bitmap centered in given coordinates
void draw_bmp_circle(bmpfile_t *bmp, int x, int y)
{

    // Data type for defining pixel colors (BGRA)
    rgb_pixel_t pixel = {255, 0, 0, 0};

    // Radius of the circle
    int radius = 30;

    // Draw the circle
    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            // If distance is smaller, point is within the circle
            if (sqrt(i * i + j * j) < radius)
            {
                /*
                 * Color the pixel at the specified (x,y) position with a factor of 20
                 * with the given pixel values
                 */
                bmp_set_pixel(bmp, x * 20 + i, y * 20 + j, pixel);
            }
        }
    }
}

// Function to erase all the bitmap
void erase_bmp(bmpfile_t *bmp)
{

    // Data type for defining pixel colors (BGRA)
    rgb_pixel_t pixel = {0, 0, 0, 0};

    // Erase the bitmap
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            bmp_set_pixel(bmp, i, j, pixel);
        }
    }
}

// Function to convert the bitmap to a matrix of 0 and 1
void bmp_to_static(bmpfile_t *bmp, rgb_pixel_t *matrix)
{
    // Convert the bitmap to a matrix
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            // Get the pixel from the bitmap
            rgb_pixel_t *pixel = bmp_get_pixel(bmp, i, j);
            // Set the pixel in the matrix
            matrix[i + width * j].alpha = pixel->alpha;
            matrix[i + width * j].red = pixel->red;
            matrix[i + width * j].green = pixel->green;
            matrix[i + width * j].blue = pixel->blue;
        }
    }
}

int main(int argc, char *argv[])
{
    // Open the log file
    logFile = fopen("log/processA.log", "w");

    // Get the current time
    time_t t = time(NULL);
    char *timeString = ctime(&t);
    timeString[strlen(timeString) - 1] = '\0';

    // Get the modality of the program from the arguments
    int modality = atoi(argv[1]);

    // Data structure for storing the bitmap file
    bmpfile_t *bmp;

    // Instantiate bitmap with the given parameters
    bmp = bmp_create(width, height, depth);

    if (bmp == NULL)
    {
        // If the bitmap is not created log and exit
        fprintf(logFile, "%s - Error while creating bitmap\n", timeString);
        exit(1);
    }

    // Shared memory name
    const char *shm_name = "/SHARED_IMAGE";

    // Size in bytes of shared memory
    int SHM_SIZE = width * height * sizeof(rgb_pixel_t);

    // Shared memory file descriptor
    int shm_fd;

    // Open the shared memory object
    if ((shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666)) == -1)
    {
        // Log the error
        fprintf(logFile, "%s - Error while opening shared memory object\n", timeString);

        // Destroy the bitmap
        bmp_destroy(bmp);

        // Exit with error
        exit(errno);
    }

    // Configure the size of the shared memory object
    if (ftruncate(shm_fd, SHM_SIZE) == -1)
    {
        // Log the error
        fprintf(logFile, "%s - Error while configuring the size of the shared memory object\n", timeString);
        // Destroy the bitmap
        bmp_destroy(bmp);
        // Close the shared memory object
        shm_unlink(shm_name); // No need to check for errors because it will exit anyway
        exit(errno);
    }

    // Map the shared memory object into the address space of the process
    rgb_pixel_t *ptr = (rgb_pixel_t *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED)
    {
        // Log the error
        fprintf(logFile, "%s - Error while mapping the shared memory object into the address space of the process\n", timeString);
        // Destroy the bitmap
        bmp_destroy(bmp);
        // Close the shared memory object
        shm_unlink(shm_name); // No need to check for errors because it will exit anyway
        exit(errno);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize UI
    init_console_ui();

    // Erase the bitmap
    erase_bmp(bmp);

    // Draw the circle in the middle of the image
    draw_bmp_circle(bmp, width / 2, height / 2);

    // Initialize the semaphore
    sem_t *sem_sh = sem_open(SEM_PATH, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem_sh == SEM_FAILED)
    {
        // Log the error
        fprintf(logFile, "%s - Error while initializing the semaphore\n", timeString);    
        // Destroy the bitmap
        bmp_destroy(bmp);
        // Unmap the shared memory object
        munmap(ptr, SHM_SIZE);
        // Close the shared memory object
        shm_unlink(shm_name); // No need to check for errors because it will exit anyway
        exit(errno);
    }

    bool error = FALSE;

    // Share the initial image
    // Protect the shared memory with the semaphore
    if (sem_wait(sem_sh) == -1)
    {
        // Log the error
        fprintf(logFile, "%s - Error while locking the semaphore\n", timeString);

        error = TRUE;
        goto cleanup;
    }

    // Convert the bitmap to a matrix
    bmp_to_static(bmp, ptr);

    // Release the semaphore
    if (sem_post(sem_sh) == -1)
    {
        // Log the error
        fprintf(logFile, "%s - Error while unlocking the semaphore\n", timeString);

        error = TRUE;
        goto cleanup;
    }

    // Variables for socket communication
    int sockfd, newsockfd, portno, clilen, n;
    struct sockaddr_in serv_addr, cli_addr;
    struct hostent *server;

    // Update the time
    t = time(NULL);
    timeString = ctime(&t);
    timeString[strlen(timeString) - 1] = '\0';

    // If the modality is server
    if (modality == 2)
    {
        // Log the event
        fprintf(logFile, "%s - Server mode\n", timeString);

        // Create a socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            // Log the error
            fprintf(logFile, "%s - Error while creating the socket\n", timeString);

            error = TRUE;
            goto cleanup;
        }

        // Initialize socket structure
        bzero((char *)&serv_addr, sizeof(serv_addr));
        portno = atoi(argv[2]);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);

        // Bind the host address
        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            // Log the error
            fprintf(logFile, "%s - Error while binding the host address\n", timeString);

            error = TRUE;
            goto cleanup;
        }

        // Start listening for the client
        listen(sockfd, 5);
        clilen = sizeof(cli_addr);

        // Update the time
        t = time(NULL);
        timeString = ctime(&t);
        timeString[strlen(timeString) - 1] = '\0';

        // Log the event
        fprintf(logFile, "%s - Waiting for client connection\n", timeString);

        // Accept connection from the client
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            // Log the error
            fprintf(logFile, "%s - Error while accepting connection from the client\n", timeString);

            error = TRUE;
            goto cleanup;
        }

        // Update the time
        t = time(NULL);
        timeString = ctime(&t);
        timeString[strlen(timeString) - 1] = '\0';

        // Log the event
        fprintf(logFile, "%s - Client connected\n", timeString);
    }
    // If modality is client
    else if (modality == 3)
    {
        // Log the event
        fprintf(logFile, "%s - Client mode\n", timeString);

        // Create a socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            // Log the error
            fprintf(logFile, "%s - Error while creating the socket\n", timeString);

            error = TRUE;
            goto cleanup;
        }

        // Get the host name
        server = gethostbyname(argv[3]);
        if (server == NULL)
        {
            // Log the error
            fprintf(logFile, "%s - Error while getting the host name\n", timeString);

            error = TRUE;
            goto cleanup;
        }

        // Initialize socket structure
        bzero((char *)&serv_addr, sizeof(serv_addr));
        portno = atoi(argv[2]);
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(portno);

        // Update the time
        t = time(NULL);
        timeString = ctime(&t);
        timeString[strlen(timeString) - 1] = '\0';

        // Log the event
        fprintf(logFile, "%s - Connecting to the server\n", timeString);

        // Connect to the server
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            // Log the error
            fprintf(logFile, "%s - Error while connecting to the server\n", timeString);

            error = TRUE;
            goto cleanup;
        }

        // Update the time
        t = time(NULL);
        timeString = ctime(&t);
        timeString[strlen(timeString) - 1] = '\0';

        // Log the event
        fprintf(logFile, "%s - Connected to the server", timeString);
    }

    // Variable to store the key to send or received
    char key[4];

    // Infinite loop
    while (TRUE)
    {
        // Update the time
        t = time(NULL);
        timeString = ctime(&t);
        timeString[strlen(timeString) - 1] = '\0';

        mvprintw(LINES - 1, 1, "Press q to quit");

        // Get input in non-blocking mode
        int cmd = getch();

        // If user resizes screen, re-draw UI...
        if (cmd == KEY_RESIZE)
        {
            if (first_resize)
            {
                first_resize = FALSE;
            }
            else
            {
                reset_console_ui();
            }
        }

        // If the user pressed q, exit
        if (cmd == 'q'){
            // Log the event
            fprintf(logFile, "%s - Quitting\n", timeString);

            break;
        }

        // If the modality is server
        if (modality == 2)
        {
            // Initialize the select function variables
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(newsockfd, &readfds);
            int maxfd = newsockfd + 1;
            struct timeval timeout = {0, 1000};

            // Wait for the client to send a byte
            int ready = select(maxfd, &readfds, NULL, NULL, &timeout);

            // If error occurred
            if (ready == -1 && errno != EINTR)
            {
                // Log the error
                fprintf(logFile, "%s - Error while waiting for the client input\n", timeString);

                error = TRUE;
                break;
            }
            // If the client sent a byte
            else if (ready > 0)
            {
                // Read the byte
                if (read(newsockfd, &key, 4) < 0)
                {
                    // Log the error
                    fprintf(logFile, "%s - Error while reading the client input\n", timeString);

                    error = TRUE;
                    break;
                }

                // Convert the read string to a integer
                int byte = atoi(key);

                // If the byte is an arrow key
                if (byte == KEY_LEFT || byte == KEY_RIGHT || byte == KEY_UP || byte == KEY_DOWN)
                {
                    // Update the time
                    t = time(NULL);
                    timeString = ctime(&t);
                    timeString[strlen(timeString) - 1] = '\0';

                    // Log the event
                    fprintf(logFile, "%s - Received arrow key\n", timeString);

                    // Move the circle
                    move_circle(byte);
                    draw_circle();

                    // Protect the shared memory with the semaphore
                    if (sem_wait(sem_sh) == -1)
                    {
                        // Log the error
                        fprintf(logFile, "%s - Error while taking the semaphore\n", timeString);

                        error = TRUE;
                        break;
                    }

                    // Erase previous circle
                    erase_bmp(bmp);

                    // Draw the circle on the bitmap in the new position
                    draw_bmp_circle(bmp, circle.x, circle.y);

                    // Convert the bitmap to a matrix
                    bmp_to_static(bmp, ptr);

                    // Release the semaphore
                    if (sem_post(sem_sh) == -1)
                    {
                        // Log the error
                        fprintf(logFile, "%s - Error while releasing the semaphore\n", timeString);

                        error = TRUE;
                        break;
                    }
                }
                // If the byte is the mouse key
                else if (byte == KEY_MOUSE)
                {
                    // Update the time
                    t = time(NULL);
                    timeString = ctime(&t);
                    timeString[strlen(timeString) - 1] = '\0';

                    // Save the image as .bmp file
                    bmp_save(bmp, "out/image.bmp");

                    // Print that the image was saved
                    mvprintw(LINES - 1, 1, "Image saved succesfully!");
                    refresh();
                    sleep(1);
                    for (int j = 0; j < COLS - BTN_SIZE_X - 2; j++)
                    {
                        mvaddch(LINES - 1, j, ' ');
                    }

                    // Log the event
                    fprintf(logFile, "%s - Picture saved\n", timeString);
                }
            }
        }
        else
        {
            // Else, if user presses print button...
            if (cmd == KEY_MOUSE)
            {
                if (getmouse(&event) == OK)
                {
                    if (check_button_pressed(print_btn, &event))
                    {
                        // If the modality is client
                        if (modality == 3)
                        {
                            // Send the print key
                            sprintf(key, "%d", KEY_MOUSE);
                            if (write(sockfd, &key, 4) < 0)
                            {
                                // Log the error
                                fprintf(logFile, "%s - Error while sending the print key\n", timeString);

                                error = TRUE;
                                break;
                            }

                            // Log the event
                            fprintf(logFile, "%s - Print command sent\n", timeString);
                        }

                        // Save the image as .bmp file
                        bmp_save(bmp, "out/image.bmp");

                        // Print that the image was saved
                        mvprintw(LINES - 1, 1, "Image saved succesfully!");
                        refresh();
                        sleep(1);
                        for (int j = 0; j < COLS - BTN_SIZE_X - 2; j++)
                        {
                            mvaddch(LINES - 1, j, ' ');
                        }

                        // Update the time
                        t = time(NULL);
                        timeString = ctime(&t);
                        timeString[strlen(timeString) - 1] = '\0';

                        // Log the event
                        fprintf(logFile, "%s - Picture saved\n", timeString);
                    }
                }
            }

            // If input is an arrow key, move circle accordingly...
            else if (cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN)
            {
                // Update the time
                t = time(NULL);
                timeString = ctime(&t);
                timeString[strlen(timeString) - 1] = '\0';

                move_circle(cmd);
                draw_circle();

                // If the modality is client
                if (modality == 3)
                {
                    // Convert the arrow key to a string
                    sprintf(key, "%d", cmd);

                    // Send the byte to the server
                    if (write(sockfd, key, 4) < 0)
                    {
                        // Log the error
                        fprintf(logFile, "%s - Error while sending the arrow key\n", timeString);

                        error = TRUE;
                        break;
                    }
                }

                // Protect the shared memory with the semaphore
                if (sem_wait(sem_sh) == -1)
                {
                    // Log the error
                    fprintf(logFile, "%s - Error while taking the semaphore\n", timeString);
                    error = TRUE;
                    break;
                }

                // Erase previous circle
                erase_bmp(bmp);

                // Draw the circle on the bitmap in the new position
                draw_bmp_circle(bmp, circle.x, circle.y);

                // Convert the bitmap to a matrix
                bmp_to_static(bmp, ptr);

                // Release the semaphore
                if (sem_post(sem_sh) == -1)
                {
                    // Log the error
                    fprintf(logFile, "%s - Error while releasing the semaphore\n", timeString);

                    error = TRUE;
                    break;
                }
            }
        }
    }

cleanup:

    // Store the errno
    int err_no = errno;

    // Free the bitmap
    bmp_destroy(bmp);

    // Unmap the shared memory object
    if (munmap(ptr, SHM_SIZE) == -1)
    {
        // Log the error
        fprintf(logFile, "%s - Error while unmapping the shared memory\n", timeString);

        exit(errno);
    }

    // Close the shared memory object
    if (shm_unlink(shm_name) == -1)
    {
        // Log the error
        fprintf(logFile, "%s - Error while closing the shared memory\n", timeString);

        exit(errno);
    }

    endwin();

    if (error)
    {
        exit(err_no);
    }

    exit(0);
}