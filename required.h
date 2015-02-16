// Optional features
#define IMP_FEATURE_ADVANCED_IO
// #define IMP_FEATURE_SLOW_FILTERS

// Comment out this in production
// #define IMP_DEBUG

// Includes
#include <stdio.h>
#include <math.h>
#include <syslog.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#ifdef IMP_FEATURE_ADVANCED_IO
#include <FreeImage.h>
#endif

// hack to avoid compilation error with freeimage < 3.16
#if FREEIMAGE_MAJOR_VERSION <= 3 && FREEIMAGE_MINOR_VERSION < 16
    #define FIF_WEBP 35
    #define FIF_JXR  36
#endif

// Errors: No
#define IMP_OK 0
// Errors: IO
#define IMP_ERROR_UNSUPPORTED   1
#define IMP_ERROR_MALLOC_FAILED 2
#define IMP_ERROR_DECODE_FAILED 3
#define IMP_ERROR_ENCODE_FAILED 4
// Errors: Bad config
#define IMP_ERROR_INVALID_ARGS      50
#define IMP_ERROR_UPSCALE           51
#define IMP_ERROR_NO_SUCH_FILTER    52
#define IMP_ERROR_NO_SUCH_WATERMARK 53
#define IMP_ERROR_TOO_BIG_TARGET    54
#define IMP_ERROR_TOO_MUCH_FILTERS  55
#define IMP_ERROR_FEATURE_DISABLED  56
// Have no idea why is this needed
#define NGX_HTTP_IMAGE_BUFFERED  0x08

// Steps
#define IMP_STEP_START     0
#define IMP_STEP_VALIDATE  1
#define IMP_STEP_DECODE    2
#define IMP_STEP_CROP      3
#define IMP_STEP_RESIZE    4
#define IMP_STEP_FILTERING 5
#define IMP_STEP_WATERMARK 6
#define IMP_STEP_INFO      7
#define IMP_STEP_ENCODE    8

// MIME types
#define IMP_MIME_INTACT  0
#define IMP_MIME_JPG    -1
#define IMP_MIME_PNG    -2
#define IMP_MIME_JSON   -3
#define IMP_MIME_ADVIO  -4
#define IMP_MIME_TEXT   -5

// Various openCV-related const
#define CV_HUE_WHEEL_RESOLUTION 180
#define CV_ALPHA     3
#define CV_RGB_RED   2
#define CV_RGB_GREEN 1
#define CV_RGB_BLUE  0
#define CV_HSV_HUE   0
#define CV_HSV_SAT   1
#define CV_HSV_VAL   2

#define KMEANS_PP_CENTERS 2


typedef struct {
    int     Code;
    int     Step;
    u_char* EncodedBytes;
    size_t  Length;
    int     MIME;
} JobResult;

typedef struct {
    char GravityX;
    char GravityY;
    int  OffsetX;
    int  OffsetY;
} Position;

typedef struct {
    unsigned int W;
    unsigned int H;
} Dimensions;

// for now, used to recover IplImage from raw blob allocated in nginx pool
typedef struct {
    CvSize  Size;
    int     Depth;
    int     Channels;
    int     Step;
    size_t  Length;
    u_char* Pointer;
} RecoverInfo;

typedef struct {
    ngx_flag_t   Enable;
    size_t       MaxSrcSize;
    ngx_str_t    WatermarkPath;
    ngx_int_t    WatermarkOpacity;
    Position*    WatermarkPosition;
    RecoverInfo* WatermarkInfo;
    Dimensions*  MaxTargetDimensions;
    ngx_int_t    MaxFiltersCount;
    ngx_flag_t   AllowExperiments;
} Config;

typedef struct {
    u_char* Image;
    u_char* Last;
    size_t  Length;
    int     ReadComplete;
    int     DataSent;
    CvMat*  ToDestruct;
} PeerContext;

typedef struct {
    IplImage* Image;
    int       Time;
    int       Dispose;
    int       TransparencyKey;
} Frame;

typedef struct {
    Frame* Frames;
    int    Count;
    int    Error;
} Album;

typedef struct {
    unsigned char* Buffer;
    long           Length;
    int            Error;
} Memory;

#ifdef IMP_FEATURE_ADVANCED_IO
typedef struct {
    unsigned char*    Buffer;
    int               Length;
    ngx_pool_t*       Pool;
    FREE_IMAGE_FORMAT Format;
    int               IsDestructive;
    int               Page;
} DecodeRequest;

typedef struct {
    Album*            Album;
    ngx_pool_t*       Pool;
    FREE_IMAGE_FORMAT Format;
    int               Flags;
} EncodeRequest;
#endif