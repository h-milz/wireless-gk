#include <stdio.h>
#include <stdlib.h>

#define FRAME_SIZE 24    // 8 channels, 3 bytes each (24 bytes total)
#define OUTPUT_SIZE 6    // 2 channels, 3 bytes each (6 bytes total)

int main() {
    unsigned char frame[FRAME_SIZE];

    // Read and process frames from stdin
    while (fread(frame, 1, FRAME_SIZE, stdin) == FRAME_SIZE) {
        // Write first 2 channels (6 bytes) to stdout
        if (fwrite(frame, 1, OUTPUT_SIZE, stdout) != OUTPUT_SIZE) {
            perror("Error writing to stdout");
            return 1;
        }
    }

    if (ferror(stdin)) {
        perror("Error reading from stdin");
        return 1;
    }

    return 0;
}

