/*
 * imaging.c
 *
 * Contains function implementations for the imaging module.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "imaging.h"
#include <openssl/sha.h>


/******************************************************************
 * Utils
 *****************************************************************/
/*
 * Parses width and height out of the given size string.
 *
 * Returns an int SUCCESS status which is 1 of successful otherwise its 0.
 *
 * If a dimension is not given, the images current dimension is used.
 * Examples: '200', 'x200', '200x200'
 */
int imaging_parse_size(const char *size, unsigned long *height, unsigned long *width) {
    //TODO: This may be able to be replaced by GetGeometry (magick/utility)
    char *token, *tmp, *endptr;
    long val;
    int offset, len, return_status;
    return_status = 1; // start successful.

    // guard requirements
    if (size == NULL ||strlen(size) == 0) {
        return 0;
    }
    // parse
    token = strchr(size, 'x');
    if (token == NULL) {
        // only width was specified. (eg: no 'x' in dimensions).
        errno = 0;
        val = strtol(size, &endptr, 10);
        if (errno != 0 || endptr == size) {
            return_status = 0;
        } else {
            *width = val;
        }
    } else {
        offset = (token - size);
        len = strlen(size);
        if (offset == 0) {
            // convert char to long
            errno = 0;
            val = strtol(token+1, &endptr, 10);
            if (errno != 0 || endptr == token) {
                return_status = 0;
            } else {
                *height = val;
            }
        } else if (offset == (len - 1)) {
            // convert char to long
            tmp = strdup(size);
            tmp[strlen(tmp) - 1] = '\0';
            errno = 0;
            val = strtol(tmp, &endptr, 10);
            if (errno != 0 || endptr == tmp) {
                return_status = 0;
            } else {
                *width = val;
            }
            free(tmp);
        } else {
            // parse width
            tmp = malloc(offset + 1);
            strncpy(tmp, size, offset);
            tmp[offset] = '\0';
            errno = 0;
            val = strtol(tmp, &endptr, 10);
            if (errno != 0 || endptr == tmp) {
                return_status = 0;
            } else {
                *width = val;
            }
            free(tmp);

            // parse height
            tmp = malloc((len - offset) + 1);
            strcpy(tmp, size+offset+1);
            tmp[len-offset] = '\0';
            errno = 0;
            val = strtol(tmp, &endptr, 10);
            if (errno != 0 || endptr == tmp) {
                return_status = 0;
            } else {
                *height = val;
            }
            free(tmp);
        }
    }

    return return_status;
}

/*
 * Extracts path, filename, extension from the given filepath.
 * Note: all have the possibility of being NULL (stranger things have happened)
 */
void imaging_explode_file_path(const char *filepath, char **path, char **file, const char **ext) {
    int len;
    char *tmp;
    // set file extension
    *ext = strrchr(filepath, '.');

    // set file
    tmp = strrchr(filepath, '/') + 1; /* walk pointer 1 to remove the '/' */
    len = strlen(tmp) - strlen(*ext);
    *file = malloc(len + 1);
    strncpy(*file, tmp, len);
    (*file)[len] = '\0';

    // set path
    len = strlen(filepath) - (strlen(*file) + strlen(*ext));
    *path = malloc(len + 1);
    strncpy(*path, filepath, len);
    (*path)[len] = '\0';
}

/******************************************************************
 * Lifecycle
 *****************************************************************/
/*
 * No internal state to worry about just setting up GraphicsMagick.
 */
void imaging_initialize() {
    InitializeMagick("");
}

/*
 * No internal state to worry about just teardown of GraphicsMagick.
 */
void imaging_destory() {
    DestroyMagick();
}

/******************************************************************
 * Actions
 *****************************************************************/
Image * imaging_action_border(Image *image, const char *action)  {
    char *token;
    long int val;
    char *size, *color, *endptr;
    Image *new_image = (Image *)NULL;
    RectangleInfo border_info;
    ExceptionInfo exception;
    const ColorInfo *border_color_info;

    // Parse border size & color from action
    token = strchr(action, '-');
    if (token == NULL) {
        DestroyImage(image);
        return new_image;
    }
    // parse size int ourt of action.
    size = malloc(token - action +1);
    strncpy(size, action, token - action);
    size[token-action] = '\0';
    errno = 0;
    val = strtol(size, &endptr, 10);
    if (errno != 0 || endptr == size) {
        // size can't be parsed.
        DestroyImage(image);
        free(size);
        return new_image;
    }
    border_info.width = border_info.height = val;
    border_info.x = border_info.y = 1;

    // parse color out of action
    color = malloc(strlen(token));
    strcpy(color, token+1);
    GetExceptionInfo(&exception);
    border_color_info = GetColorInfo(color, &exception);
    if (border_color_info == (ColorInfo *)NULL) {
        // failed to find color.
        DestroyImage(image);
        DestroyExceptionInfo(&exception);
        free(size);
        free(color);
        return new_image;
    }
    image->border_color = border_color_info->color;

    // apply border
    GetExceptionInfo(&exception);
    new_image = BorderImage(image, &border_info, &exception);
    // free memory
    free(size);
    free(color);
    DestroyImage(image);
    DestroyExceptionInfo(&exception);
    return new_image;
}

Image * imaging_action_crop(Image *image, const char *action)  {
    Image *new_image = (Image *)NULL;
    ExceptionInfo exception;
    RectangleInfo geometry;
    unsigned long height, width;

    // default to current value
    height = image->rows;
    width = image->columns;

    if (!imaging_parse_size(action, &height, &width)) {
        DestroyImage(image);
        return new_image;
    }
    // clamp size to less than current.
    height = height < image->rows? height:image->rows;
    width = width < image->columns? width: image->columns;

    // build geometry of a centered box within the existing image.
    geometry.x = (image->columns - width) / 2;
    geometry.y = (image->rows - height) / 2;
    geometry.width = width;
    geometry.height = height;

    // crop the image.
    GetExceptionInfo(&exception);
    new_image = CropImage(image, &geometry, &exception);

    // free memory
    DestroyImage(image);
    DestroyExceptionInfo(&exception);

    return new_image;
}

Image * imaging_action_filter(Image *image, const char *action)  {
    //TODO: Needs implemented. No one really uses this from what I can tell
    // but still probably should be there for completeness sake.
    DestroyImage(image);
    return (Image *)NULL;
}

Image * imaging_action_resize(Image *image, const char *action)  {
    double org_aspect_ratio, cur_aspect_ratio;
    unsigned long height, width;
    Image *new_image = (Image *)NULL;
    Image *thumb_image = (Image *)NULL;
    RectangleInfo geometry;
    ExceptionInfo exception;

    // default to current value
    geometry.x = geometry.y = 0;
    height = image->rows;
    width = image->columns;

    // bail if the size can't be parsed.
    if (!imaging_parse_size(action, &height, &width)) {
        DestroyImage(image);
        return new_image;
    }

    // clamp size to less than current.
    height = height < image->rows? height:image->rows;
    width = width < image->columns? width: image->columns;

    // geometry of crop
    geometry.height = height;
    geometry.width = width;

    // height & width will be used to first thumbnail the image
    // with the original aspect ratio intact.
    org_aspect_ratio = (double)image->columns / image->rows;
    cur_aspect_ratio = (double)width / height;

    if (cur_aspect_ratio < org_aspect_ratio) {
        // thumbnail height & crop width
        width = height * org_aspect_ratio;
    } else if (cur_aspect_ratio > org_aspect_ratio) {
        // thumbnail width & crop height
        height = width / org_aspect_ratio;
    }

    // perform thumb
    GetExceptionInfo(&exception);
    thumb_image = ThumbnailImage(image, width, height, &exception);
    if (thumb_image != (Image *)NULL) {
        // center geometry
        geometry.x = (thumb_image->columns - geometry.width) / 2;
        geometry.y = (thumb_image->rows - geometry.height) / 2;
        // perform crop
        GetExceptionInfo(&exception);
        new_image = CropImage(thumb_image, &geometry, &exception);
        DestroyImage(thumb_image);
    }

    // free memory
    DestroyImage(image);
    DestroyExceptionInfo(&exception);
    return new_image;
}

Image * imaging_action_scale(Image *image, const char *action)  {
    Image *new_image = (Image *)NULL;
    ExceptionInfo exception;
    unsigned long height, width;

    // default to current value
    height = image->rows;
    width = image->columns;

    // bail if the size can't be parsed.
    if (!imaging_parse_size(action, &height, &width)) {
        DestroyImage(image);
        return new_image;
    }

    // clamp size to less than current.
    height = height < image->rows? height:image->rows;
    width = width < image->columns? width: image->columns;

    // scale the image.
    GetExceptionInfo(&exception);
    new_image = ResizeImage(image, width, height, BoxFilter, image->blur, &exception);

    // free memory
    DestroyImage(image);
    DestroyExceptionInfo(&exception);
    return new_image;
}

/*
 * Thumbnails an image to the specified size.
 */

Image * imaging_action_thumbnail(Image *image, const char *action) {
    unsigned long height, width;
    Image *new_image = (Image *)NULL;
    ExceptionInfo exception;
    GetExceptionInfo(&exception);

    // default to 0 (we'll treat this like NULL).
    height = 0;
    width = 0;
    // bail if the size can't be parsed.
    if (!imaging_parse_size(action, &height, &width)) {
        DestroyImage(image);
        return new_image;
    }

    // if only one dimension was specified compute the other one
    // preserving the aspect ratio.
    double aspect_ratio = (double)image->columns / image->rows;
    if (height == 0) {
        height = width / aspect_ratio;
    } else if (width == 0) {
        width = height * aspect_ratio;
    } else if (width / height != aspect_ratio) {
        // PIL enforces aspect ratio even if you define width & height.
        // for backwards compatibility, do the same.
        // IMHO: This is probably an error in PIL's implementation.
        height = width / aspect_ratio;
    }

    // perform thumbnail.
    new_image = ThumbnailImage(image, width, height, &exception);

    // free memory
    DestroyImage(image);
    DestroyExceptionInfo(&exception);
    return new_image;
}


Image * imaging_action_echo(Image *image, const char *action) {
    printf("In imaging_action_func: args: %s\n", action);
    return image;
}

/******************************************************************
 * Core
 *****************************************************************/
/*
 * Returns a pointer to a function or NULL no function is found.
 * The returned function has a type of:
 *      Image * func(Image *image, const char *action);
 */
imaging_action_func_ptr imaging_get_action_func(const char *code) {
    static const char action_codes[] = "b c f r s t ";
    static imaging_action_func_ptr action_funcs[6] = {
        imaging_action_border,      // border
        imaging_action_crop,        // crop
        imaging_action_filter,      // filter
        imaging_action_resize,      // resize
        imaging_action_scale,       // scale
        imaging_action_thumbnail,   // thumbnail
    };
    int offset;
    char *cmdptr;
    // get offset of the action code
    cmdptr = strpbrk(action_codes, code);
    if (cmdptr != NULL) {
        // compute array offset and index into action_funcs with it.
        offset = (cmdptr - action_codes) / 2;
        return (*action_funcs[offset]);
    }
    return NULL;
}

int
imaging_actions_allowed(const char *action, const char *salt,
    const char *hash,const char *white_list)
{
    int i, passed_sec = 1;
    unsigned char result[SHA_DIGEST_LENGTH];
    char computed_hash[(SHA_DIGEST_LENGTH * 2) + 1];
    char *string;

    // if salt is defined then do security checks
    if (salt != NULL && strlen(salt) > 0) {
        // see if action string in in white_list
        passed_sec = (strstr(white_list, action + 1) != NULL);
        // if action wasn't in white_list and a hash was passed in
        // see if the hash is valid.
        if (!passed_sec && hash != NULL && strlen(hash) > 0) {
            // compute hash out of own (action_string + salt)
            string = malloc(strlen(salt) + strlen(action) + 1);
            strcpy(string, action);
            strcat(string, salt);
            string[strlen(salt) + strlen(action)] = '\0';
            SHA1((const unsigned char *)string, strlen(string), result);
            free(string);
            // expand the sha1 hash into hex format.
            for(i = 0; i < SHA_DIGEST_LENGTH; i++) {
                sprintf(computed_hash + i*2, "%02x", result[i]);
            }
            passed_sec = (strcmp(hash, computed_hash) == 0);
        }
    }
    return passed_sec;
}


/**
 * Applies actions in the action string to the Image.
 */
void imaging_apply_actions(Image **image, char *actions) {
    char *action;
    int len,
        offset = 0,
        max_len = strlen(actions);

    // parse the actions one segment at a time.
    while (offset <= max_len && (*image) != (Image *)NULL) {
        // get action segment
        len = strcspn(offset+actions, "_");
        action = malloc(len + 1);
        strncpy(action, offset+(actions), len);
        action[len] = '\0';

        imaging_action_func_ptr func = imaging_get_action_func(action);
        // apply action or bail if none existed.
        if (func == NULL) {
            (*image) = (Image *)NULL; // break out of loop next time around
        } else {
            (*image) = func(*image, action+1);
            // cleanup the memory of the current_image
            // walk our offset to the next action.
            offset = offset+(len+1);
        }
        free(action);
    }
}

/*
 * Returns an Image * (which can point to NULL) along with updating image_info
 * and exception (if there was an exception encountered).
 *
 * Tries to create an image by decoding image_info->filename into an original
 * image & a series of operations to perform on that image.
 */
Image * imgaging_create_image(
    ImageInfo *image_info, ExceptionInfo *exception,
    const char *salt, const char *hash,
    const unsigned long quality,
    const char *white_list,
    const int write_to_disk)
{
    Image *image = (Image *)NULL;
    char *action_str;
    char *path;
    char *file;
    char *newfile;
    const char *ext;
    char *orginal_filename = strdup(image_info->filename);
    (void) imaging_explode_file_path(image_info->filename, &path, &file, &ext);
    int path_len = strlen(path), ext_len = strlen(ext);
    int file_len;
    int passed_sec = 1;

    // split file on '_'
    action_str = strchr(file, '_');
    while(action_str != NULL) {
        // build image filename
        file_len = (action_str - file);
        newfile = malloc(path_len + file_len + ext_len + 1);
        strcpy(newfile, path);
        strncat(newfile, file, file_len);
        newfile[path_len + file_len] = '\0';
        strcat(newfile, ext);
        (void) strcpy(image_info->filename, newfile);
        free(newfile);

        if (IsAccessible(image_info->filename)) {
            // try loading the image
            GetExceptionInfo(exception);
            image = ReadImage(image_info, exception);
            if (exception->severity != UndefinedException) {
                break;
            }
        } else {
            // add one more action_str segment to the filename.
            action_str = strchr(action_str + 1, '_');
        }

        // stop if file was found and opened it.
        if (image != (Image *)NULL) {
            break;
        }
    }

    passed_sec = imaging_actions_allowed(action_str, salt, hash, white_list);

    // if security failed and we have an image release it now.
    if (!passed_sec && image != (Image *)NULL) {
        DestroyImage(image);
        image = (Image *)NULL;
    }

    // Apply transformations if image exists
    if (image != (Image *)NULL) {
        imaging_apply_actions(&image, ++action_str); // remove '_' prefix.
    }


    if (image != (Image *)NULL) {
        // set the filename back to its original
        strcpy(image_info->filename, orginal_filename);
        strcpy(image->filename, orginal_filename);
        // write the image to disk
        image_info->quality = quality;
        // Remove any profile data (stuff like EXIF) before writing to disk.
        ProfileImage(image, "*", 0, 0, 0);
        if (write_to_disk != 0) {
            WriteImage(image_info, image);
        }
    }

    // memory cleanup
    free(file);
    free(path);
    free(orginal_filename);
    return image;
}

/******************************************************************
 * Public API
 *****************************************************************/
/*
 * ngx_imaging_module interface. This method provides an easy to use
 * interface from the context of an nginx handler module.
 */
void
imgaging_get_image_data(
    const char *filepath,
    unsigned char **data, size_t *data_length,
    char **content_type, size_t *content_type_length,
    const char *salt, const char *hash,
    const unsigned long quality,
    const char *white_list,
    const int write_to_disk)
{
    // locals
    Image *image = (Image *)NULL;
    ImageInfo *image_info;
    ExceptionInfo exception;

    // create ImageInfo and set filepath
    image_info = CloneImageInfo((ImageInfo *) NULL);
    (void) strcpy(image_info->filename, filepath);

    // try to load/create the image.
    GetExceptionInfo(&exception);
    if (IsAccessible(filepath)) {
        image = ReadImage(image_info, &exception);
    } else {
        // the file did not exist on disk. try creating it.
        image = imgaging_create_image(
            image_info, &exception,
            salt, hash, quality, white_list, write_to_disk
        );
    }

    // if we got an image extract the data from it.
    if (image != (Image *)NULL) {
        *content_type = MagickToMime(image->magick);
        *content_type_length = strlen(*content_type) - 1;
        *data = ImageToBlob(image_info, image, data_length, &exception);
        DestroyImage(image);
    }

    // cleanup
    if (image_info != (ImageInfo *)NULL) {
        DestroyImageInfo(image_info);
    }
    if (&exception != (ExceptionInfo *) NULL) {
        DestroyExceptionInfo(&exception);
    }
}

