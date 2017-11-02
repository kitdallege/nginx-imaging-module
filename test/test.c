/*
 * Very simple test suite for the Imaging library.
 * Runs a series of tests, stops at the first failing test otherwise prints
 * the number of passing tests.
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GraphicsMagick.
#include <imaging.h>
#include <magick/api.h>


// Testing macros
// Credit for macros goes to: http://www.jera.com/techinfo/jtns/jtn002.html
#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); printf("."); fflush(stdout); tests_run++; if (message) return message; } while (0)
#define mu_test_type static char * 
#define mu_return_success return 0
extern int tests_run;
int tests_run = 0;

// Tests for: void explodeFilePath(const char *filepath, char **path, char **file, const char **ext);
mu_test_type test_explodeFilePath() {
    //mu_assert("ext of [file] is .jpg", isImageFileType(".jpg"));
    mu_return_success;
}


// Tests for: imaging_get_image_data
mu_test_type test_imaging_get_image_data() {
    unsigned char *data = NULL;
    char *content_type = NULL;
    size_t data_length, content_type_length;
    const unsigned long quality = 70;
    const char *white_list = "";
    const int write_to_disk = 0;
    const char *files[] = {
        "docroot/img/lg-image.jpg",

        // border
        "docroot/img/lg-image_b5-black.jpg",
        "docroot/img/lg-image_b5-red.jpg",

        // crop
        "docroot/img/lg-image_c400.jpg",
        "docroot/img/lg-image_c400x400.jpg",
        "docroot/img/lg-image_cx400.jpg",

        // resize
        "docroot/img/lg-image_r125x125.jpg",
        "docroot/img/lg-image_r200x500.jpg",
        "docroot/img/lg-image_r500x200.jpg",
        "docroot/img/lg-image_r500x500.jpg",
        // scale
        "docroot/img/lg-image_s400.jpg",
        "docroot/img/lg-image_s400x400.jpg",
        "docroot/img/lg-image_sx400.jpg",

        // thumbnail
        "docroot/img/lg-image_t400.jpg",
        "docroot/img/lg-image_t400.jpg",
        "docroot/img/lg-image_t400x400.jpg",
        "docroot/img/lg-image_tx400.jpg",

        // church tests.
        "docroot/img/scaled.insidechurch_t120.jpg",
        "docroot/img/scaled.insidechurch_t190.jpg",
        "docroot/img/scaled.insidechurch_t198.jpg",
        "docroot/img/scaled.insidechurch_t200.jpg",
        "docroot/img/scaled.insidechurch_t300.jpg",
        "docroot/img/scaled.insidechurch_t320.jpg",
        "docroot/img/scaled.insidechurch_t653.jpg",
        "docroot/img/scaled.insidechurch_t653x653.jpg",
        "docroot/img/scaled.insidechurch_t655.jpg",
        "docroot/img/scaled.insidechurch_tx50.jpg",
    };

    int i;
    int num_of_files = sizeof(files) / sizeof(char *);
    for (i = 0; i < num_of_files; ++i) {
        imgaging_get_image_data(
            files[i],
            &data, &data_length,
            &content_type, &content_type_length,
            "", "",
            quality, white_list, write_to_disk
        );
        if (data == NULL) {
            printf("%s was NULL!\n", files[i]);
            mu_assert("Failed test", 0);
        }
        tests_run++;
        printf(".");
        fflush(stdout);
        free(data);
        free(content_type);
    }

    mu_return_success;
}

mu_test_type test_imaging_get_image_data_hash() {
    unsigned char *data = NULL;
    char *content_type = NULL;
    size_t data_length, content_type_length;
    const unsigned long quality = 70;
    const char *white_list = "";
    const int write_to_disk = 0;
    imgaging_get_image_data(
        "docroot/img/lg-image_t600.jpg",
        &data, &data_length,
        &content_type, &content_type_length,
        "On the other hand, the camel has not evolved to smell good.", "42b0fb247f69dabe2ae440581a34634cbc5420f3",
        quality, white_list, write_to_disk
    );
    mu_assert("SHA1 security hash.", data != NULL);
    free(data);
    free(content_type);
    mu_return_success;
}

mu_test_type test_imaging_get_image_data_white_list() {
    unsigned char *data = NULL;
    char *content_type = NULL;
    size_t data_length, content_type_length;
    const unsigned long quality = 70;
    const char *white_list = "t200 r480 t200x200 t700";
    const int write_to_disk = 0;
    // salt is provided so hash check is enabled but no hash is provided.
    // since the image action is in the white_list is should be allowed
    imgaging_get_image_data(
        "docroot/img/lg-image_t700.jpg",
        &data, &data_length,
        &content_type, &content_type_length,
        "On the other hand, the camel has not evolved to smell good.", "",
        quality, white_list, write_to_disk
    );
    mu_assert("white_list should have allowed this.", data != NULL);
    free(data);
    free(content_type);
    tests_run++;
    printf(".");
    fflush(stdout);

    data = NULL;
    imgaging_get_image_data(
        "docroot/img/lg-image_t666.jpg",
        &data, &data_length,
        &content_type, &content_type_length,
        "On the other hand, the camel has not evolved to smell good.", "",
        quality, white_list, write_to_disk
    );
    mu_assert("white_list should have disallowed this.", data == NULL);
    mu_return_success;
}

// Test runner.
mu_test_type all_tests(){
    //explodeFilePath
    mu_run_test(test_explodeFilePath);
    
    //GetImageData
    mu_run_test(test_imaging_get_image_data);
    mu_run_test(test_imaging_get_image_data_hash);
    mu_run_test(test_imaging_get_image_data_white_list);
    mu_return_success;
}

// Entry Point
int main(void) {
    (void) imaging_initialize();
    printf("Running test suite.\n");
    char *results = all_tests();
    printf("\n");
    printf("%d tests ran\n", tests_run);
    if (results != 0) {
        printf("Failed Test: %s\n", results);
    } else {
        printf("All Tests Passed!\n");
    }   
    (void) imaging_destory();
    return results != 0;
}

