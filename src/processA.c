#include "./../include/processA_utilities.h"
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
    // Get the modality of the program from the arguments
    int modality = atoi(argv[1]);
    
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

    // Configure the size of the shared memory object
    if (ftruncate(shm_fd, SHM_SIZE) == -1)
    {
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
        error = TRUE;
        goto cleanup;
    }

    // Convert the bitmap to a matrix
    bmp_to_static(bmp, ptr);

    // Release the semaphore
    if (sem_post(sem_sh) == -1)
    {
        error = TRUE;
        goto cleanup;
    }

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

        // Else, if user presses print button...
        else if (cmd == KEY_MOUSE)
        {
            if (getmouse(&event) == OK)
            {
                if (check_button_pressed(print_btn, &event))
                {
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
                }
            }
        }

        // If input is an arrow key, move circle accordingly...
        else if (cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN)
        {
            move_circle(cmd);
            draw_circle();

            // Protect the shared memory with the semaphore
            if (sem_wait(sem_sh) == -1)
            {
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
                error = TRUE;
                break;
            }
        }
    }

cleanup:

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