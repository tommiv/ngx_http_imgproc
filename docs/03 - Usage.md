#Usage

Most of image operations (except watermark) performs via query string. You can expose it directly, or wrap in your nginx config as it showed above. Simple query example is:

    # this will create lightweight 120x120px thumbnail
    http://example.com/imgproc/sample.jpg?crop=1,1,c,c&resize=120,0&quality=60

All operations arguments are comma separated. In case of unrecoverable errors in query (typos or unexisted operations, wrong argument types or count, arguments outside allowed ranges) IMP will return *400 Bad Request*. For optional arguments you **must** set all arguments previous to what you changing. I.e. if operation has required arg0 and arg1 and optional arg2, arg3 and arg4, and you want to control arg3, your method call will be *operation=arg0,arg1,arg2,arg3*.

##Crop

Crop arguments includes dimensions and gravity. Dimension can be relative (ratio crop) or in pixels (exact size crop). Gravity shows where to reposition image after crop. Gravity is optional, default values is center-top.

**Dimensions**. Pair of values in order x-axis,y-axis. Relative is just positive integers. Absolute values has *px* postfix.

**Gravity**. Pair of chars in order x-axis,y-axis. Optional. Default is *c,t*. Options:

    c – center
    t – top
    b – bottom
    l – left
    r – right
    
Samples:
    
    # square, reposition to center-top
    crop=1,1
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/Q5eZrdn.jpg)
    
    # square, reposition to right-bottom
    crop=1,1,r,b
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/ittVdj5.jpg)

    # 16:9, to left-top
    crop=16,9,l,t
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/o7tvatj.jpg)
    
    # absolute values
    crop=400px,200px,c,t
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/MuJhcmY.jpg)

##Resize

**Width,Height**. To make IMP keep aspect ratio just set one of this and leave other as 0 or empty. Set both values will lead to resize to exact dimension with no respect to aspect ratio.

**Allow Upscale**. Optional. By default, set to dimensions larger than image size will just skip resize. If you faced rare case when you want to upscale picture, put *up* keyword to 3rd argument. Remember that IMP will still respect *imgproc\_max\_target\_dimensions* config option.

IMP will use CV\_INTER\_CUBIC filter for upscale and CV\_INTER\_AREA for downscale.

    # just resize to 1000px width, keep aspect ratio
    resize=1000
    # resize to exact 1000x600px, don't keep ratio
    resize=1000,600
    # resize to 300 height, keep ratio
    resize=0,300
    # allow upscale
    resize=1400,0,up

##Filters
All filters has format *filter-%name%=%args%*. 

If you don't need filters, it's better to competely disable it with *imgproc\_max\_filters\_count 0* option.

All filters mark as *experimental* will work only if config variable *imgproc\_allow\_experiments* set to *on*. IMP will return *400 Bad Request* otherwise.

Filters marked as *instagram* are converted from command-line snippets described [here](http://code.tutsplus.com/tutorials/create-instagram-filters-with-php--net-24504). Note: it's not even close – I failed to replicate it precisely even in IMP interface, while openCV do things completely different.

Filters allways applies in order you write it in query string, so order it wisely.

---
###flip

Mirrors image.

**Direction**. It's bitmask where less significant (the right one) is vertical flip (around X axis) and most significant is horizontal (around Y axis). Can be one of constants 00, 01, 11, 10. All other values will cause *400 Bad Request*.

    # vertical
    filter-flip=01
![Imgur](http://i.imgur.com/RLk8eY1.jpg) ![Imgur](http://i.imgur.com/sxCmswi.jpg)

    # horizontal
    filter-flip=10
![Imgur](http://i.imgur.com/RLk8eY1.jpg) ![Imgur](http://i.imgur.com/p7He5xA.jpg)    
    
    # both
    filter-flip=11
![Imgur](http://i.imgur.com/RLk8eY1.jpg) ![Imgur](http://i.imgur.com/SEe3XBc.jpg)

----------

###rotate

Does exactly what is says. 

**Degrees**. Only orthogonal values supported, therefore is 90, 180, 270. All other values will throw *400 Bad Request*.

    # rotate upside down
    fiter-rotate=180

---
###modulate
Operations with image perception in HSV colorspace.

**Hue**. Required. This is shift around color wheel, in degrees. openCV uses 180 deg resolution, so this value can be in range [0; 180].
    
**Saturation**. Required. Any integer.

    [ 101 –  xxx] – oversaturation
    [   0 –   99] – desaturation
    [ 100 –  100] – intact
    [-xxx –   -1] – funny side effect of negative addition

**Brightness**. Required. Any positive integer. Values <= 0 causes totally black image, so IMP will throw *400 Bad Request*.

    # grayscale
    filter-modulate=100,0,100
![Imgur](http://i.imgur.com/aRMnAAD.jpg) ![Imgur](http://i.imgur.com/LX7eA4a.jpg)

    # acid colors
    filter-modulate=100,500,500
![Imgur](http://i.imgur.com/aRMnAAD.jpg) ![Imgur](http://i.imgur.com/I5GMhTD.jpg)

    # psycho
    filter-modulate=60,100,100
![Imgur](http://i.imgur.com/aRMnAAD.jpg) ![Imgur](http://i.imgur.com/lFEIYff.jpg)

---
###colorize

Renders color overlay. Useful in combination with grayscale in particular.

**Color**. Required. Any valid RGB hex value without hash (#) mark.

**Opacity**. Optional. Normalized float in range 0–1. Default is 0.5.

    # half-strong red overlay
    filter-colorize=ff0000
![Imgur](http://i.imgur.com/92ntoI8.jpg) ![Imgur](http://i.imgur.com/7PloMlW.jpg)
    
    # faded with warm sepia tone
    filter-modulate=0,0,100&filter-colorize=704214,0.6
![Imgur](http://i.imgur.com/92ntoI8.jpg) ![Imgur](http://i.imgur.com/fAfa0fu.jpg)

---
###blur
Applies blur with CV_GAUSSIAN kernel.

**Sigma**. Required. Any positive float. While openCV has some more arguments to pass in cvSmooth, sigma is the one which almost linearly describes effect intensity.

    # light blur
    filter-blur=2
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/a6HhLzB.jpg)

    # medium blur
    filter-blur=6
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/1t7UJtj.jpg)

    # heavy blur
    filter-blur=12
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/m8s79ZE.jpg)

    # what is this I don't even blur
    filter-blur=25
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/XijznxG.jpg)


*Note*: blur execution time is proportional to sigma, but it's not so critical on any reasonable value.

---
###gamma
Does what it said.

**Amount**. Required. Any float. 0 means black image, 1 means do nothing. Negative values creates psycho side-effects.

    # more light
    filter-gamma=1.3
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/M3WCTle.jpg)

    # less light
    filter-gamma=0.5
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/oJHDcOF.jpg)

    # just for fun
    filter-gamma=-0.1
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/uUO8Gmc.jpg)

---
###experimental:vignette
Makes lens vignette effect. Converted from [Browny's "fun with filters" project](https://github.com/browny/filters-fun).

**Intensity**. Required. Any float. Means how heavy pixels will be shadowed. Note that resulting pixels are in 4th power, so negative values are pointless. Try to start with 0.8 and adjust as you want.

**Radius**. Optional. Any float. Default is 1. Usually this about 75-125% of intensity. Really small values cause cool side-effects such as ripple or web overlay.

    # mild
    filter-vignette=0.8
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/ev6eaae.jpg)

    # hard
    filter-vignette=4,3
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/mdVLfVO.jpg)

    # fast ripple
    filter-vignette=4,0.1
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/vAey4eB.jpg)

    # web
    filter-vignette=4,0.00005
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/cCEE0g4.jpg)

---
###experimental:rainbow
Shifts any pixel to its closest rainbow color. Useless for smooth images with gradients, will just uglify picture.

**Saturation**. Enum *full|mid|pale*. Changes overall picture saturation after shift.

![Imgur](http://i.imgur.com/ATY1LQQ.jpg) ![Imgur](http://i.imgur.com/tczIZSk.jpg) ![Imgur](http://i.imgur.com/ZS7ZxDb.jpg) ![Imgur](http://i.imgur.com/V3c9uuE.jpg)

---
###experimental:scanline
Stylized as old tube TV picture with interlaced lines. Just for fun.

**Lightness**. Required. Normalized float in range [0;1]. Changes lightness of scan lines for black to white.

**Opacity**. Optional. Float in range [0; 1]. Default is 0. Means that lines follows pixel color under it (1) or not (0).

**Frequency**. Optional. Any positive integer. Default is 1. Shows how often scan line will be rendered. Literally, "show line every N lines".

**Height**. Optional. Any positive integer. Default is 1. Show height of scan line in pixels.

    # Normal
    filter-scanline=0
![Imgur](http://i.imgur.com/ATY1LQQ.jpg) ![Imgur](http://i.imgur.com/bZI2nGp.jpg)

    # Big white lines
    filter-scanline=1,0,10,2
![Imgur](http://i.imgur.com/ATY1LQQ.jpg) ![Imgur](http://i.imgur.com/eVf7jfz.jpg)

---
###experimental:instagram:gotham
Noir filter for very sad of us. Black border can be easily added via CSS. There is no controllable parameters for this filter. Just pass anything after =.

    filter-gotham=1
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/2GAsM6S.jpg)

---

###experimental:instagram:lomo
Totally distorted and popular. There is no controllable parameters for this filter. Just pass anything after =. While this looks better with vignette, it don't applies automatically. Add *filter-vignette=...* after lomo, if you need.

    # standard
    filter-lomo=1&filter-vignette=1
![Imgur](http://i.imgur.com/rKXWrLE.jpg) ![Imgur](http://i.imgur.com/x9ETdbL.jpg)

    # standalone lomo (no vignette)
    filter-lomo=1
![Imgur](http://i.imgur.com/rKXWrLE.jpg) ![Imgur](http://i.imgur.com/lU8GzHN.jpg)

---
    
###experimental:instagram:kelvin
The yellow one. There is no controllable parameters for this filter. Border is not included since it's applies from file and should be redistributed. It actually has flow in blend mode, so it eats red color, but I believe no one cares.

    # no comments
    filter-kelvin=1
![Imgur](http://i.imgur.com/rKXWrLE.jpg) ![Imgur](http://i.imgur.com/XsOc6uT.jpg)

    
##Format

While it's rare case, you still can convert your image to one of popular web format. Content-Type header will be changed respectively.

**Format**. This is the only argument. Required. jpg|png is supported by openCV. webp is supported via 3rd part lib. You need to build IMP with uncommented flag IMP\_FEATURE\_WEBP in required.h. 

    format=png
    format=jpg
    format=webp
    
##Quality

Low quality can save bandwidth, but it can bring to picture... low quality. It looks like imgproc don't give a shit on change quality for png/gif, so this option useful only for jpg.

**Value**. This is the only argument. Required.

For jpg/webp:
Positive integer in range [0; 100], where 100 is best quality and biggest file. Default is 86.

For png:
Positive integer in range [0; 9] where 9 is smallest file. However, in my test environment only value 0 and 3 makes different results. #todo: figure it out.
    
    quality=80
    format=png&quality=9
    
##Info

You can ask IMP for info about image instead of image itself, by adding `info=1` to query. Data compiles just before compression step, so any changes such as resize/crop/filters are represented in it. Info returns in JSON format, and for now, contains this members:
    
**width**. Int, in pixels

**height**. Int, in pixels

**brightness**. Int, in percents; shows weighted average image brightness, can be used to determine which text color draw over. 0 for totally black and 100 for totally white.