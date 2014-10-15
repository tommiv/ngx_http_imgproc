void       OnEnvStart();
void       OnEnvDestroy();
void       PeerDestructor(void* data);
int        Crop(IplImage** pointer, char* args);
int        Resize(IplImage** pointer, char* args, Config* config);
int        PrepareWatermark(Config* cfg, ngx_pool_t* pool);
int        Watermark(IplImage* image, Config* config);
u_char*    Info(IplImage* image, ngx_pool_t* pool);
JobResult* RunJob(u_char* blob, size_t size, ngx_http_request_t* req, Config* config);