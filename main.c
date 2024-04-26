#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <conio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

//[amount of Frames][rows per Frame][chars per row]
char frames[60 * 220][30][121];

void readFrames() {
    FILE *fptr;
    fptr = fopen("frames", "r");
    if (fptr == NULL) {
        printf("Cannot open file. Press any key to terminate the program...");
        getch();
        exit(0);
    }

    char frameContents[121];
    int currentFrame = 0;
    int i = 0;
    while (fgets(frameContents, sizeof(frameContents), fptr)) {
        char *row = strdup(frameContents);
        if (row == NULL) {
            printf("Memory allocation failed. Press any key to terminate the program...");
            getch();
            exit(0);
        }
        if (i % 2 == 0) {
            strcpy(frames[currentFrame / 30][currentFrame % 30], row);
            currentFrame++;
        }
        free(row);
        i++;
    }

    fclose(fptr);
}

int frameIndex = 0;

void clearScreen() {
    printf("\e[1;1H\e[2J");
}

void drawFrame() {
    char (*frameRows)[121] = frames[frameIndex];

    for (size_t i = 0; i < 30; i++) {
        printf("%s\n", frameRows[i]);
    }
}

int main() {
    int fps;
    readFrames();
    printf("Enter the number of frames per second: ");
    scanf("%d", &fps);
    float frameTime = 1000.0 / fps; // Time to wait between frames in milliseconds

    // Calculate total frames for 60 seconds video
    size_t frameAmount = fps * (60 * 3 + 39);

    printf("Frame time: %f\n", frameTime);
    printf("You entered: %d\n", fps);

    // Display initial frame
    drawFrame();
    
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);

    // Loop to display frames with proper timing
    for (frameIndex = 1; frameIndex < frameAmount; frameIndex++) {
        QueryPerformanceCounter(&start);

        clearScreen();
        drawFrame();

        QueryPerformanceCounter(&end);
        long long elapsed_ns = (end.QuadPart - start.QuadPart) * 1000000 / frequency.QuadPart;

        // Adjust sleep time
        long long remaining_ns = (long long)(frameTime * 1000) - elapsed_ns;
        if (remaining_ns > 0) {
            // Busy-wait loop for fine-grained timing
            long long target_end = end.QuadPart + (remaining_ns * frequency.QuadPart) / 1000000;
            while (end.QuadPart < target_end) {
                QueryPerformanceCounter(&end);
            }
        }
    }

    printf("Press any key to exit . . . ");
    getch();

    return 0;
}
