# Usage

Most of image operations (except watermark) are configured via query string. You can expose it directly, or wrap in your nginx config, as shown above. Simple query example is:

```
# this will create lightweight 120x120px thumbnail
http://example.com/imgproc/sample.jpg?crop=1,1,c,c&resize=120,0&quality=60
```

All operations arguments are comma-separated. In case of unrecoverable errors in query (typos, unexisting operations, wrong arguments types or count, arguments outside allowed ranges) IMP will return *400 Bad Request*. When using optional arguments, you **must** bypass all preceding args. I.e. if operation has required arg0 and arg1 and optional arg2, arg3 and arg4, and you want to set arg3, your method call should be *operation=arg0,arg1,arg2,arg3*.

## Crop

Crop arguments include dimensions and gravity. Dimension can be a relative (crop ratio) or absolute value (size in pixels). Gravity defines how to reposition image after crop. Gravity args are optional, defaults to center-top.

**Dimensions**. Pair of values in order x-axis,y-axis. Relative values are just positive integers. Absolute values have *px* postfix.

**Gravity**. Pair of chars in order x-axis,y-axis. Optional. Defaults to *c,t*. Options:

```
c – center
t – top
b – bottom
l – left
r – right
```

Samples:

```
# square, reposition to center-top
crop=1,1
```
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/Q5eZrdn.jpg)

```
# square, reposition to right-bottom
crop=1,1,r,b
```
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/ittVdj5.jpg)

```
# 16:9, to left-top
crop=16,9,l,t
```
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/o7tvatj.jpg)

```
# absolute values
crop=400px,200px,c,t
```
![Imgur](http://i.imgur.com/XigrLA5.jpg) ![Imgur](http://i.imgur.com/MuJhcmY.jpg)

## Resize

**Width and height**. To make IMP preserve aspect ratio, set a value for only one dimension and 0 for another one. Setting both values to positive integers will perform resize to exact given size, ignoring aspect ratio.

**Allow Upscale**. Optional. By default, upscale is disabled. If you want to upscale an image, set *up* keyword as 3rd argument. Note, that IMP will still respect *imgproc\_max\_target\_dimensions* config option in such case.

IMP will use `CV_INTER_CUBIC` filter for upscale, `CV_INTER_AREA` for downscale and `CV_INTER_NN` for GIF files in both axes.

```
# just resize to 1000px width, keep aspect ratio
resize=1000
# resize to 1000x600px, don't preserve ratio
resize=1000,600
# resize to 300px height, keep ratio
resize=0,300
# allow upscale
resize=1400,0,up
```

## Filters

All filters have format *filter-%name%=%args%*.

If you don't need filters, it's better to disable them with *imgproc\_max\_filters\_count 0* option.

Filters marked as *experimental* will only work if config variable *imgproc\_allow\_experiments* is set to *on*. Otherwise IMP will return *400 Bad Request*.

Filters marked as *instagram* are converted from command-line snippets described [here](http://code.tutsplus.com/tutorials/create-instagram-filters-with-php--net-24504). Note: result is not even close - I failed to replicate it precisely even in IMP interface, while OpenCV does things in a completely different way.

Filters are always applied in order you pass them in query string, so order'em wisely.

***

### flip

Mirrors an image.

**Direction**. It's a bitmask where less significant (the right one) is vertical flip (around X axis) and most significant is horizontal (around Y axis). Can be one of constants: `00`, `01`, `11`, `10`. All other values will cause *400 Bad Request*.

```
# vertical
filter-flip=01
```
![Imgur](http://i.imgur.com/RLk8eY1.jpg) ![Imgur](http://i.imgur.com/sxCmswi.jpg)

```
# horizontal
filter-flip=10
```
![Imgur](http://i.imgur.com/RLk8eY1.jpg) ![Imgur](http://i.imgur.com/p7He5xA.jpg)

```
# both
filter-flip=11
```
![Imgur](http://i.imgur.com/RLk8eY1.jpg) ![Imgur](http://i.imgur.com/SEe3XBc.jpg)

***

### rotate

Does exactly what it says.

**Degrees**. Only orthogonal values are supported: 90, 180, 270. All other values will lead to *400 Bad Request*.

```
# rotate upside down
fiter-rotate=180
```

***

### modulate

Operations with image perception in HSV colorspace.

**Hue**. Required. This is shift around color wheel, in degrees. OpenCV uses 180 deg resolution, so this value can be in range [0; 180].

**Saturation**. Required. Any integer.

```
[ 101 –  xxx] – oversaturation
[   0 –   99] – desaturation
[ 100 –  100] – intact
[-xxx –   -1] – funny side effect of negative addition
```

**Brightness**. Required. Any positive integer. Values <= 0 lead to totally black image, so IMP will throw *400 Bad Request*.

```
# grayscale
filter-modulate=100,0,100
```
![Imgur](http://i.imgur.com/aRMnAAD.jpg) ![Imgur](http://i.imgur.com/LX7eA4a.jpg)

```
# acid colors
filter-modulate=100,500,500
```
![Imgur](http://i.imgur.com/aRMnAAD.jpg) ![Imgur](http://i.imgur.com/I5GMhTD.jpg)

```
# psycho
filter-modulate=60,100,100
```
![Imgur](http://i.imgur.com/aRMnAAD.jpg) ![Imgur](http://i.imgur.com/lFEIYff.jpg)

***

### colorize

Renders color overlay. Can be especially good in combination with grayscale filter.

**Color**. Required. Any valid RGB hex value without hash (#) mark.

**Opacity**. Optional. Normalized float in range 0–1. Defaults to `0.5`.

```
# half-strong red overlay
filter-colorize=ff0000
```
![Imgur](http://i.imgur.com/92ntoI8.jpg) ![Imgur](http://i.imgur.com/7PloMlW.jpg)

```
# faded with warm sepia tone
filter-modulate=0,0,100&filter-colorize=704214,0.6
```
![Imgur](http://i.imgur.com/92ntoI8.jpg) ![Imgur](http://i.imgur.com/fAfa0fu.jpg)

***

### blur

Applies blur with `CV_GAUSSIAN` kernel.

**Sigma**. Required. Any positive float. While OpenCV has some more arguments to pass in cvSmooth, sigma is the one which almost linearly describes effect intensity.

```
# light blur
filter-blur=2
```
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/a6HhLzB.jpg)

```
# medium blur
filter-blur=6
```
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/1t7UJtj.jpg)

```
# heavy blur
filter-blur=12
```
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/m8s79ZE.jpg)

```
# what is this I don't even blur
filter-blur=25
```
![Imgur](http://i.imgur.com/MlXqlbI.jpg) ![Imgur](http://i.imgur.com/XijznxG.jpg)

*Note*: blur execution time is proportional to sigma, but it's not so critical on any reasonable value.

***

### gamma

Does what it says.

**Amount**. Required. Any float. 0 means black image, 1 means "do nothing". Negative values may cause psycho side-effects.

```
# more light
filter-gamma=1.3
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/M3WCTle.jpg)

```
# less light
filter-gamma=0.5
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/oJHDcOF.jpg)

```
# just for fun
filter-gamma=-0.1
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/uUO8Gmc.jpg)

***

### contrast

Does what it says.

**Amount**. Required. Positive float. 1 means "do nothing". Low values means black image.

```
# more contrast
filter-contrast=1.5
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/S0KzHF6.jpg)

```
# less contrast
filter-contrast=0.5
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/NtsGR8S.jpg)

***

### gradmap

Applies gradient mapping – brightness of each pixel maps to a value of gradient provided in arguments. This effect is close to the "Gradient map" feature in Photoshop, but based on linear gradient generator.

**Gradient colors**. Required. List of comma-separated hex RGB colors, 2-8 values. Only 6-chars format is supported. _Note: order of color values is important: left values will map to most dark colors of original picture and vice versa._

```
# mild
filter-gradmap=306090,eecc00
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/bpJlobm.jpg)

```
# inverse previous sample
filter-gradmap=306090,eecc00
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/ZFFc7Wr.jpg)

```
# multicolor in random order
filter-gradmap=203040,8080aa,cc60dd,000000
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/McL1bGs.jpg)

***

### experimental:vignette

Makes lens vignette effect. Ported from [Browny's "fun with filters" project](https://github.com/browny/filters-fun).

**Intensity**. Required. Any float. Defines how heavily will pixels be shadowed. Note that resulting pixels are in 4th power, so negative values are pointless. I would recommend to start with value of 0.8 and adjust it.

**Radius**. Optional. Any float. Default is 1. Usually this should be about 75-125% of intensity. Really small values cause cool side-effects such as ripple or web overlay.

```
# mild
filter-vignette=0.8
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/ev6eaae.jpg)

```
# hard
filter-vignette=4,3
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/mdVLfVO.jpg)

```
# fast ripple
filter-vignette=4,0.1
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/vAey4eB.jpg)

```
# web
filter-vignette=4,0.00005
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/cCEE0g4.jpg)

***

### experimental:rainbow

Shifts any pixel to its closest rainbow color. Useless for smooth images with gradients, will just uglify the picture.

**Saturation**. Enum *full|mid|pale*. Changes overall picture saturation after shift.

![Imgur](http://i.imgur.com/ATY1LQQ.jpg) ![Imgur](http://i.imgur.com/tczIZSk.jpg) ![Imgur](http://i.imgur.com/ZS7ZxDb.jpg) ![Imgur](http://i.imgur.com/V3c9uuE.jpg)

***

### experimental:scanline

A-la old TV tube picture with interlaced lines. Just for fun.

**Lightness**. Required. Normalized float in range [0;1]. Changes lightness of scan lines from black to white.

**Opacity**. Optional. Float in range [0; 1]. Default is 0. Full transparecny when 1.

**Frequency**. Optional. Any positive integer. Default is 1. Literally, "draw a line every N lines".

**Height**. Optional. Any positive integer. Default is 1. Height of a line in pixels.

```
# Normal
filter-scanline=0
```
![Imgur](http://i.imgur.com/ATY1LQQ.jpg) ![Imgur](http://i.imgur.com/bZI2nGp.jpg)

```
# Big white lines
filter-scanline=1,0,10,2
```
![Imgur](http://i.imgur.com/ATY1LQQ.jpg) ![Imgur](http://i.imgur.com/eVf7jfz.jpg)

***

### experimental:instagram:gotham
Noir filter for sad souls. Black border can be easily added via CSS. There are no parameters for this filter. Just pass anything after `=`.

```
filter-gotham=1
```
![Imgur](http://i.imgur.com/GwBpQXT.jpg) ![Imgur](http://i.imgur.com/2GAsM6S.jpg)

***

### experimental:instagram:lomo

Very popular filter with strong image color distortion. There are no parameters for this filter, just pass anything after `=`. Looks better in combination with vignette, so try it with *filter-vignette=...* after lomo, if you want.

```
# standard
filter-lomo=1&filter-vignette=1
```
![Imgur](http://i.imgur.com/rKXWrLE.jpg) ![Imgur](http://i.imgur.com/x9ETdbL.jpg)

```
# standalone lomo (no vignette)
filter-lomo=1
```
![Imgur](http://i.imgur.com/rKXWrLE.jpg) ![Imgur](http://i.imgur.com/lU8GzHN.jpg)

***

### experimental:instagram:kelvin

The yellow one. There are no parameters for this filter. Border is not included. It actually has flow in blend mode, so it "eats" red color, but I believe no one cares.

```
# no comments
filter-kelvin=1
```
![Imgur](http://i.imgur.com/rKXWrLE.jpg) ![Imgur](http://i.imgur.com/XsOc6uT.jpg)

## Format

You can convert your image to one of popular web formats. Content-Type header will be set respectively. Some formats below support only output (for example, you cannot _read_ picture from JSON, only _write_ info in JSON format to output). Some formats support quality options.

```
format=jpg&quality=70
format=webp
format=json
format=text&quality=wide
```

#### Support list

- **BasicIO**. Supported by OpenCV, so this works out-of-the box.
    - jpg. Quality is integer in range [0; 100], where bigger is better.
    - png. Quality is integer in range [0; 9], where bigger is smaller file. However, it looks like OpenCV doesn't directly use this option.
- **AdvancedIO**. Supported by FreeImage and available only when this feature is on. Introduces support of animated (or not) GIFs, WEBP (v3.16 or later), JPEG2K etc. IMP supports subset of FreeImage formats which work without any special image preparations. List of formats _not_ implemented by FIF can be found in advancedio.h.
    - webp, j2k, jp2. Quality is integer in range [0; 100], where bigger is better.
    - targa, bmp. Quality is string "rle" for run-length encoding compression or any other for disabling compression.
    - tiff. Quality is string: "deflate", "lzw" for lossless, "jpeg" for lossy or "none" for disabling compression.
    - jpeg. Just for fun, you can use FreeImage jpeg encoder instead of OpenCV's one. I didn't found any visually notable difference between OpenCV and FreeImage results, probably both of them use libjpeg inside. Quality param is the same as for jpg/webp.
    - gif. Does not support any quality options. Supports animation frames. Important note: GIF uses diff-compression between frames, leaving non-updated parts of new frame transparent, so it can be compressed effectively. This transparency can be broken by some filters. For now, such "destructive" filters are blur and vignette. In this case IMP will restore a full representation of each frame. This helps to get a proper result, but file size will be increased dramatically. Same restoration is performed when you extract page from multipaged image (see "Page" section of this file). Also, keep in mind that time will be multiplied to frames count, and additional calculation of index table will take time, so processing big gifs is generally slow operation.

    ```
    /sponge.gif?resize=300&filter-modulate=60,70,80
    ```

![Imgur](http://i.imgur.com/uk5VI6O.gif) ![Imgur](http://i.imgur.com/MIhjdov.gif)

- **Non-image**. Some additional features, which do not return image itself.
    - json. Returns image info in json format.
        - _width_. Int, in pixels
        - _height_. Int, in pixels
        - _brightness_. Int, in percents. Average weighted image brightness. Useful for cases when you need to place some text above a cover image and want it to be black for light-toned images and white for "darker" ones. 0 stands for totally black image and 100 for totally white. If image is multipaged, calculation is done for first frame.
        - _count_. Int, shows frame count of a multipaged image. Equals to 1 for single-paged images.

    - text. Returns ASCII-art representation of an image in plain text. Quality param is string: "wide" will use the wide (70 character) density table, while any other value will make IMP use the basic table. Ironically, the basic table usually gives a better-looking result. You may want to resize image before asciifying it. It is also a good idea to reduce its height, because of most fonts proportions.

    ```
    format=text
    ```
![Imgur](http://i.imgur.com/4N1qBnUl.png)

## Page

This directive allows to get an exact frame from a multipaged image. If `page` argument value is above total frames count, 1st frame will be returned.

```
crop=1,1,c,c&filter-gotham=1&page=10
```

![Imgur](http://i.imgur.com/uk5VI6O.gif)
![Imgur](http://i.imgur.com/5tpbquW.jpg)