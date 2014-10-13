#define cvPxAddr(src, x, y, c) (src->widthStep * y + x * src->nChannels + c)
#define cvSetComponent(src, x, y, c, val) (src->imageData[cvPxAddr(src, x, y, c)] = val)
#define cvGetRawComponent(src, x, y, c) (src->imageData[cvPxAddr(src, x, y, c)])
#define cvGetComponent(src, x, y, c) ((u_char)cvGetRawComponent(src, x, y, c))

int   StartsWith(char* haystack, const char* needle);
char* RewindArgs(char* pointer, const char* stopper);
void  OutputErrorString(const char* msg, ngx_http_request_t* r);
char* CopyNgxString(ngx_str_t string);

float Dist(CvPoint a, CvPoint b);
float GetMaxDisFromCorners(CvSize imgSize, CvPoint center);