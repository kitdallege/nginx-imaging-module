#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GraphicsMagick.
#include <magick/api.h>
#include <imaging.h>

static int NUMBER_OF_ITERATIONS = 1000;

int main(void) {
    (void) imaging_initialize();
    int i = 0;
    unsigned char *data = NULL;
    char *content_type = NULL;
    size_t data_length, content_type_length;
    const unsigned long quality = 70;
    const char *white_list = "";
    const int write_to_disk = 0;

    // create the same image over and over
    for (i = 0; i < NUMBER_OF_ITERATIONS; ++i) {
        imgaging_get_image_data(
            "docroot/img/lg-image_t200.jpg",
            &data, &data_length,
            &content_type, &content_type_length,
            "", "",
            quality, white_list, write_to_disk
        );
        if (data == NULL) {
            printf("thumb was NULL!\n");
        }
        free(data);
        free(content_type);
    }
    (void) imaging_destory();
    return EXIT_SUCCESS;
}
