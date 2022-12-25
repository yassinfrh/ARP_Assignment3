#include "./../include/processB_utilities.h"
#include <bmpfile.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>

#define SEM_PATH "/sem_SHARED_IMAGE"

// Dimensions of the image
const int width = 1600;
const int height = 600;
const int depth = 4;

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

// Function to convert the matrix of 0 and 1 to a bitmap
void static_to_bmp(rgb_pixel_t *matrix, bmpfile_t *bmp)
{
    // Loop through the matrix
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            // Get the pixel from the matrix
            rgb_pixel_t pixel = matrix[i + width * j];
            // Set the pixel in the bitmap
            bmp_set_pixel(bmp, i, j, pixel);
        }
    }
}

// Function to find center of the cirlce in the bitmap
void find_center(bmpfile_t *bmp, int *x, int *y)
{
    // Variable to store the length of the current circumference rope
    int length = 0;

    // Variable to store the length of the longest circumference rope
    int max_length = 0;

    // Cycle through the bitmap
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            // Get the pixel at the specified (x,y) position
            // Data type for defining pixel
            rgb_pixel_t *pixel = bmp_get_pixel(bmp, i, j);

            // If the pixel is not black
            if (pixel->blue != 0 || pixel->green != 0 || pixel->red != 0)
            {
                // Increment the length of the current circumference rope
                length++;
            }
            else
            {
                // If the current circumference rope is longer than the longest one
                if (length > max_length)
                {
                    // Update the longest circumference rope
                    max_length = length;

                    // Update the center of the circle
                    *x = j / 20;
                    *y = (i - length / 2) / 20;
                }

                // Reset the length of the current circumference rope
                length = 0;
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    // Data structure for storing the bitmap file
    bmpfile_t *bmp;

    // Instantiate bitmap with the given parameters
    bmp = bmp_create(width, height, depth);

    if (bmp == NULL)
    {
        // If the bitmap is not created, exit
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
        // Destroy the bitmap
        bmp_destroy(bmp);

        // Exit with error
        exit(errno);
    }

    // Map the shared memory object into the address space of the process
    rgb_pixel_t *ptr = (rgb_pixel_t *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED)
    {
        // Destroy the bitmap
        bmp_destroy(bmp);
        // Close the shared memory object
        shm_unlink(shm_name); // No need to control the return value because the program is exiting anyway with errno
        exit(errno);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize UI
    init_console_ui();

    // Variables to store the center of the circle
    int x, y;

    // Initialize the semaphore
    sem_t *sem_sh = sem_open(SEM_PATH, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem_sh == SEM_FAILED)
    {
        // Destroy the bitmap
        bmp_destroy(bmp);
        // Unmap the shared memory object
        munmap(ptr, SHM_SIZE);
        // Close the shared memory object
        shm_unlink(shm_name); // No need to control the return value because the program is exiting anyway with errno
        exit(errno);
    }

    bool error = FALSE;

    // Infinite loop
    while (TRUE)
    {

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

        else
        {
            // Protect the shared memory with a semaphore
            if (sem_wait(sem_sh) == -1)
            {
                error = TRUE;
                break;
            }

            // Erase the bitmap
            erase_bmp(bmp);

            // Convert the matrix to a bitmap
            static_to_bmp(ptr, bmp);

            // Release the semaphore
            if (sem_post(sem_sh) == -1)
            {
                error = TRUE;
                break;
            }

            // Find the center of the circle
            find_center(bmp, &x, &y);

            mvaddch(x, y, '0');
            refresh();
        }
    }
    // Free the bitmap
    bmp_destroy(bmp);

    // Unmap the shared memory object
    if (munmap(ptr, SHM_SIZE) == -1)
    {
        exit(errno);
    }

    // Close the shared memory object
    if (shm_unlink(shm_name) == -1)
    {
        exit(errno);
    }

    endwin();

    if (error)
    {
        exit(errno);
    }
    exit(0);
}