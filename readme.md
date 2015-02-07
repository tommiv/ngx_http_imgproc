This is a nginx module for runtime image processing based on OpenCV. It has same basic features as [ngx\_http\_image\_filter\_module
](http://nginx.org/en/docs/http/ngx\_http\_image\_filter\_module.html), plus provides some advanced configurations and implements a few filters, which can be potentially used on the web. Also OpenCV is pretty fast and can beat libgd, ImageMagick and, in some cases, even VIPS. The module is named ngx\_http\_imgproc, so I will call it just "IMP" hereafter.

**Things to know about IMP:**

1. It is tested by me only. Every kind of trouble are possible: unexpected behavior, lack of features, misleading documents or even memory leaks. Therefore it's a good idea not to use it in production without a couple of days of testing. I would say IMP is in alpha stage now.

2. Nginx architecture allows IMP to run really fast, but it is sensitive to heavy calculations. I hardly recommend to compile IMP as a separated nginx binary and run it as separated master process to avoid overall site slowdown in case of heavy image processing.

3. Docs are located in /docs/*.md.

4. You'll need some experience in building apps from sources. I've covered it as much as possible in "Installation.md", but you never know.

**Pros:**

- Advantages of fast IO by nginx
- Easy results caching with nginx
- Fast generic image manipulations (crop, resize, watermark) with openCV
- Wide formats support (**animated GIFs** included) via FreeImage
- Set of cool and useless filters by me and (mostly) StackOverflow
- Dynamic processing configuration via GET params

**Cons:**

- Lack of stability
- Each request execution time affects whole system performance
- Complicated installation
