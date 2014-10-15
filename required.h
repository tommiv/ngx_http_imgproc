// Optional features
#define IMP_FEATURE_WEBP

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
#ifdef IMP_FEATURE_WEBP
#include <webp/encode.h>
#endif

// Errors: No
#define IMP_OK 0
// Errors: IO
#define IMP_ERROR_UNSUPPORTED   1
#define IMP_ERROR_MALLOC_FAILED 2
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
#define IMP_STEP_CROP      1
#define IMP_STEP_RESIZE    2
#define IMP_STEP_FILTERING 3
#define IMP_STEP_WATERMARK 4
#define IMP_STEP_INFO      5
#define IMP_STEP_FORMAT    6
#define IMP_STEP_QUALITY   7
#define IMP_STEP_GET_BLOB  8

// MIME types
#define IMP_MIME_INTACT 0
#define IMP_MIME_JPG    1
#define IMP_MIME_PNG    2
#define IMP_MIME_WEBP   3
#define IMP_MIME_JSON   4

// Various openCV-related const
#define CV_HUE_WHEEL_RESOLUTION 180

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