/**
 *  imaging.h
 *
 *  Contains function prototypes for the Imaging module.
 */
#ifndef _IMAGING_H_INCLUDED_
#define _IMAGING_H_INCLUDED_

#define MAGICK_IMPLEMENTATION 1
#include <magick/api.h>

#ifdef  __cplusplus
extern "C" {
#endif

// typedef for a pointer to a image action function
typedef Image * (*imaging_action_func_ptr)(Image *, const char *);

Image * imaging_action_border(Image *image, const char *action);

Image * imaging_action_crop(Image *image, const char *action);

Image * imaging_action_filter(Image *image, const char *action);

Image * imaging_action_resize(Image *image, const char *action);

Image * imaging_action_scale(Image *image, const char *action);

/*
 * Thumbnails an image to the specified size.
 */
Image * imaging_action_thumbnail(Image *image, const char *action);

/**
 * Initialize the library. Required before calling any other methods.
 */
void imaging_initialize(void);

/**
 * Cleanup after the library (eg: don't leak memory, resource handles).
 */
void imaging_destory(void);

/**
 * Given a char * filepath will extract the (path, filename, extension)
 */
void imgaging_explode_file_path(const char *filepath, char **path, char **file, const char **ext);

/*
 * Parses width and height out of the given size string
 */
int imaging_parse_size(const char *size, unsigned long *height, unsigned long *width);

/*
 *
 */
int imaging_actions_allowed(const char *action, const char *salt, const char *hash, const char *white_list);

/*
 * Returns a pointer to an action function for the given action code.
 */
imaging_action_func_ptr imaging_get_action_func(const char *code);

/*
 * Applies the given transformations encoded in the actions string to the
 * Image.
 */
void imaging_apply_actions(Image **image, char *actions);

/**
 * Returns an Image created from the filename specified in the passed image_info.
 * Return (Image *)NULL if it was unable to create the image.
 */
Image * imgaging_create_image(
    ImageInfo *image_info, ExceptionInfo *exception,
    const char *salt, const char *hash,
    /* image quality */
    const unsigned long quality,
    /* space separated list of the allowed actions when
     * salt is defined but no hash is given. */
    const char *white_list,
    /* flag: write created images to disk? */
    const int write_to_disk
);

/**
 * Main api for the ngx_imaging_module
 *
 * data == NULL if there was a problem.
 */
void imgaging_get_image_data(
    /* file being requested */
    const char *filepath,
    /* data returned & its length */
    unsigned char **data, size_t *data_length,
    /* mime type and its length */
    char **content_type, size_t *content_type_length,
    /* security salt & the current hash */
    const char *salt, const char *hash,
    /* image quality */
    const unsigned long quality,
    /* space separated list of the allowed actions when
     * salt is defined but no hash is given. */
    const char *white_list,
    /* flag: write created images to disk? */
    const int write_to_disk
);

#ifdef  __cplusplus
    }
#endif

#endif
