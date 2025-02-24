// Author: John Kolb <jhkolb@umn.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "string_vector.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_SIZE 4

int strvec_init(strvec_t *vec) {
    vec->length = 0;
    vec->capacity = INITIAL_SIZE;
    vec->data = malloc(INITIAL_SIZE * sizeof(char *));
    if (vec->data == NULL) {
        return -1;
    }

    return 0;
}

void strvec_clear(strvec_t *vec) {
    if (vec->capacity == 0) {
        return;
    }
    for (int i = 0; i < vec->length; i++) {
        free(vec->data[i]);
    }
    free(vec->data);

    vec->length = 0;
    vec->capacity = 0;
}

int strvec_add(strvec_t *vec, const char *s) {
    // If vector was previously cleared, need to reinitialize
    if (vec->capacity == 0) {
        if (strvec_init(vec) != 0) {
            return -1;
        }
    }

    if (vec->length == vec->capacity) {
        // Expand underlying array
        char **new_data = realloc(vec->data, 2 * vec->capacity * sizeof(char *));
        if (new_data == NULL) {
            return -1;
        } else {
            vec->data = new_data;
        }
        vec->capacity = vec->capacity * 2;
    }

    if ((vec->data[vec->length] = malloc((strlen(s) + 1) * sizeof(char))) == NULL) {
        return -1;
    }
    strcpy(vec->data[vec->length], s);
    vec->length++;
    return 0;
}

char *strvec_get(const strvec_t *vec, unsigned i) {
    if (i >= vec->length) {
        return NULL;
    }

    return vec->data[i];
}

int strvec_find(const strvec_t *vec, const char *s) {
    for (int i = 0; i < vec->length; i++) {
        if (strcmp(vec->data[i], s) == 0) {
            return i;
        }
    }
    return -1;
}

void strvec_take(strvec_t *vec, unsigned n) {
    if (n >= vec->length) {
        return;
    }

    for (int i = n; i < vec->length; i++) {
        free(vec->data[i]);
    }
    vec->length = n;
}
