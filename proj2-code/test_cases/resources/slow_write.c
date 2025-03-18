// Author: John Kolb <jhkolb@umn.edu>
// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int limit = 0;
    int delay;
    FILE *fout;

    if (argc < 3) {
        printf("Usage: <max_num> <delay> [out_file]\n");
        return 1;
    }
    limit = atoi(argv[1]);
    delay = atoi(argv[2]);

    if (argc >= 4) {
        fout = fopen(argv[3], "w");
        if (fout == NULL) {
            perror("fopen");
            return 1;
        }
    } else {
        fout = stdout;
    }

    for (int i = 1; i <= limit; i++) {
        fprintf(fout, "%d\n", i);
        sleep(delay);
    }
}
