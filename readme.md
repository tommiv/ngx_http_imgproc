This is nginx module for runtime image processing based on OpenCV. It has basic features of [ngx\_http\_image\_filter\_module
](http://nginx.org/en/docs/http/ngx\_http\_image\_filter\_module.html) but also provides some deep settings for it and implements a few filters which can be potentially used in web. Also OpenCV is pretty fast and can overcome libgd, ImageMagick or, in some cases, even VIPS. Module called ngx\_http\_imgproc, so I will call it just "IMP" below.

Things to know about IMP:

1. It tested only by me. All kinds of trouble is possible: unexpected behavior, lack of some features, misleading documents or even memory leaks. Therefore it's a good idea to not try it in production without a couple of days of testing. I would say IMP is an alpha stage now.

2. Nginx architecture makes it really fast, but also sensitive to heavy calculations. I really insist to compile IMP in separated nginx binary and run it as separated master process to avoid overall site slowdown in case of heavy image processing.

3. Docs are in /docs/*.md.

4. You'll need some experience in building apps from sources. I tried to cover as much as possible in "Installation.md", but you'll never know.

5. You can do anything you want with the code, including using its parts or whole project, redistribute and stuff, but don't blame me if something works wrong. 