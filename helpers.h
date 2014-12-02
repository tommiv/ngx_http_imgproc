#define cvPxAddr(src, x, y, c) (src->widthStep * y + x * src->nChannels + c)
#define cvSetComponent(src, x, y, c, val) (src->imageData[cvPxAddr(src, x, y, c)] = val)
#define cvGetRawComponent(src, x, y, c) (src->imageData[cvPxAddr(src, x, y, c)])
#define cvGetComponent(src, x, y, c) ((u_char)cvGetRawComponent(src, x, y, c))

int   StartsWith(char* haystack, const char* needle);
int   ByteCompare(unsigned char* haystack, unsigned char* needle, int length);
char* RewindArgs(char* pointer, const char* stopper);
void  OutputErrorString(const char* msg, ngx_http_request_t* r);
char* CopyNgxString(ngx_str_t string);
char* CopyString(char* string);

float Dist(CvPoint a, CvPoint b);
float GetMaxDisFromCorners(CvSize imgSize, CvPoint center);

void RGB2HSV(IplImage* image);
void HSV2RGB(IplImage* image);

#define len(x) (x == NULL ? 0 : sizeof(x) / sizeof(x[0]))
#define MIN3(x,y,z)  ((y) <= (z) ? ((x) <= (y) ? (x) : (y)) : ((x) <= (z) ? (x) : (z)))
#define MAX3(x,y,z)  ((y) >= (z) ? ((x) >= (y) ? (x) : (y)) : ((x) >= (z) ? (x) : (z)))