#include <stdio.h>
#include "debug.h"

int main(int argc, char **argv) {
    FILE *file1, *file2;
    unsigned char byte1, byte2;
    long position = 0;
    int differences = 0;

    // Open the binary files for reading
    file1 = fopen(argv[1], "rb");
    file2 = fopen(argv[2], "rb");

    if (file1 == NULL || file2 == NULL) {
        printf("Failed to open binary files.\n");
        return 1;
    }

    // Compare files byte by byte
    while (fread(&byte1, sizeof(unsigned char), 1, file1) &&
           fread(&byte2, sizeof(unsigned char), 1, file2)) {
        if (byte1 != byte2) {
            printf("Difference at position %ld: 0x%02X (file1) vs 0x%02X (file2)\n",
                   position, byte1, byte2);
            differences++;
        }
        position++;
    }

    if (differences == 0) {
        printf("No differences found out of %lx.\n", position);
    } else {
        printf("Total differences: %x out of %lx\n", differences, position);
    }

    // Close the files
    fclose(file1);
    fclose(file2);

    return 0;
}