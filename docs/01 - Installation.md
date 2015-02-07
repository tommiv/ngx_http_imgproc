# Installation

## Prerequisites

All tips in this doc are related to Ubuntu 12.04. If you use less popular OS, it's up to you to do all the stuff below in some other way.

First, you will need **git**. If you don't have git, just install it with the package manager:

```
sudo apt-get install git
```

***

## Install OpenCV from sources

You will need OpenCV. It's [official site](http://opencv.org/) suggests to compile it from source.

```
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install build-essential cmake libgtk2.0-dev python-dev python-numpy libavcodec-dev libavformat-dev libswscale-dev libjpeg-dev libpng-dev libtiff-dev libjasper-dev
```

Download sources. You can do it with wget, but the source is stored on SourceForge which has its own handler for downloads. It's much easier to download the source code with your favorite browser and put it to your server via an SFTP client. Get your copy [here](http://sourceforge.net/projects/opencvlibrary/files/opencv-unix/2.4.9/opencv-2.4.9.zip/download). I put mine into /var/opt/src/OpenCV.

It's compile time:

```
cd /var/opt/src/OpenCV
cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local .
```

If configure was ok, run:

```
make
sudo make install
```

This will take... a while.

**[Optional]**. Probably your ldconfig already knows about `/usr/local/lib`, but if you get a message that dynamic library is not found, put

```
/usr/local/lib
```

to /etc/ld.so.conf and update binding:

```
sudo ldconfig
```

***

## Install FreeImage

FreeImage library provides support for huge list of input/output image formats, including WEBP, TIFF and GIF (with **animation support**). If you don't need this, just comment out _IMP\_FEATURE\_ADVANCED\_IO_ flag at the top of required.h file and skip this section. Without FreeImage IMP restricted to OpenCV codec, and therefore supports only JPEG and PNG.

#### Via package manager

Before you start, there is a **huge caveat**: it will probably install version 3.15.x of this library. WEBP therefore will be unavailable. (Official site also says: _It (v3.16) also provides better support for RAW, JPEG-2000, BMP, SGI, ICO, PSD, TIFF, TGA, GIF formats_).

If you are OK with this, just run:

```
sudo apt-get install libfreeimage-dev
```

Otherwise...

#### Compiling from source

1. Go to http://freeimage.sourceforge.net/download.html and copy link to the source. Let's say it is http://downloads.sourceforge.net/freeimage/FreeImage3160.zip

2. Download and unpack it.

  ```
  cd /var/opt/src
  wget http://downloads.sourceforge.net/freeimage/FreeImage3160.zip
  unzip FreeImage3160.zip
  ```

3. Compile and install

  ```
  cd FreeImage
  make
  sudo make install
  ```

***

## Get nginx sources

```
cd ..
git clone https://github.com/nginx/nginx.git
```

**[Optional].** You can download `pcre`, `openssh` and/or `zlib`. They are used by some nginx modules, but not required when you build IMP-enabled nginx as a separate service. As for me, I downloaded `pcre`.

***

## Get IMP sources

```
git clone https://github.com/tommiv/ngx_http_imgproc.git
```

***

## It's build time

I assume you are still in `/var/opt/src` and you have all the required sources here: nginx, optional pcre|libssh|zlib and IMP.

```
cd nginx
```

Write the configure line for nginx:

```
./configure \
    --prefix=/var/opt/bin/nginx-imp \
    --sbin-path=./ \
    --conf-path=./conf/nginx.conf \
    --pid-path=./nginx.pid \
    --without-http_rewrite_module \
    --add-module=../ngx_http_imgproc
```

You can just copy/paste it if `/var/opt/bin/nginx-imp` binary path is OK for you. If you've downloaded any additional libs like zlib, you can add them to configure command, as described [here](http://wiki.nginx.org/InstallOptions).

Wait until `configure` is finished. If you don't see an "error" word, everyting is OK.

Now is your chance. Type:

```
make
```

If you're lucky enough, you'll see a compiler message saying everything is complete. Check for it:

```
objs/nginx
```

You'll see a message about not founding log and config files. It's OK, proceed to configuration step.

If you see "not found" message, compilation was unsuccessful.

If you see `libdc1394 error: Failed to initialize libdc1394` – don't worry too much. This is not an error actually, it's a warning. I wasn't able to found simple fix for this, but it has no consequences, it's just annoying.

***

## Put the binary where it belongs

Make dirs. Copy nginx binary and config stub to the working directory

```
mkdir /var/opt/bin/nginx-imp
mkdir /var/opt/bin/nginx-imp/conf
mkdir /var/opt/bin/nginx-imp/logs
cp objs/nginx /var/opt/bin/nginx-imp/
cp conf/* /var/opt/bin/nginx-imp/conf
```

We're done here. You can go to the destination folder

```
cd /var/opt/bin/nginx-imp
```

and start configuring the service.

***

## [Optional] Add IMP to upstart

Based on <s>actual events</s> [this article](http://articles.slicehost.com/2007/10/17/ubuntu-lts-adding-an-nginx-init-script).

If you used the same binary path as me, just put `/docs/nginx-imp` script to `/etc/init.d/`. Otherwise open it first and replace DAEMON and PIDFILE values at lines 15-16 with your own ones.

Make the script executable

```
sudo chmod +x /etc/init.d/nginx-imp
```

Update run levels

```
sudo /usr/sbin/update-rc.d -f nginx-imp defaults
```

That's all, now you can use

```
service nginx-imp start
service nginx-imp stop
service nginx-imp reload
```

to control IMP.

If you don't want to do this crazy stuff, you can run IMP with

```
/var/opt/bin/nginx-imp/nginx
```

And use signals to stop/restart the service

```
/var/opt/bin/nginx-imp/nginx -s %signal%
i.e.
/var/opt/bin/nginx-imp/nginx -s stop
```

***

## Update/rebuild

Pull new nginx and/or IMP sources

```
cd /var/opt/src/nginx
git pull origin master

cd /var/opt/src/ngx_http_imgproc
git pull origin master
```

Run make inside

```
cd /var/opt/src/nginx
make
```

If you see errors, try to re-run `./configure` as described in **It's build time** section and run `make` again.

Replace binary

```
service nginx-imp stop
cp objs/nginx /var/opt/bin/nginx-imp/
service nginx-imp start
```

***

## Run inside docker
There's no prepared image, but it's definetely possible to build it. Here is a sample of how it could work, but you'll probably want to modify paths/ips/configs etc.

0. Install docker if needed

    ```
    sudo apt-get install docker.io
    ```

1. Go to the nginx source folder and create a folder for docker files

    ```
    cd /var/opt/src/nginx
    mkdir docker && cd docker
    ```

2. Create build.sh file with this content:

    ```
    #! /bin/sh

    cp ../objs/nginx .
    cp /usr/local/lib/libopencv_core.so.2.4 .
    cp /usr/local/lib/libopencv_imgproc.so.2.4 .
    cp /usr/local/lib/libopencv_highgui.so.2.4 .
    cp /usr/lib/libfreeimage.so.3 .
    mkdir -p /etc/nginx-imp/conf
    mkdir -p /etc/nginx-imp/logs
    cp -f ../conf/* /etc/nginx-imp/conf

    docker.io build -t nginx_imp .
    ```

    **Important**: building openCV, FreeImage and nginx itself inside docker is too complex, slow and leads to too big docker image, so we just copy pre-built libs and executable to image. However, code above assumes that openCV is 2.4.x branch and FreeImage is 3.x branch and both libs built from sources as it described in this manual. It's up to you to check if this libs really exists on its places. Note you can use `readelf -d nginx | grep NEEDED` to get list of dynamic libs required by nginx-imp executable.

3. Create Dockerfile with the following content:

    ```
    FROM ubuntu
    MAINTAINER John Doe <john@example.com>

    RUN apt-get update && apt-get install -y libgtk2.0-dev libavcodec-dev libavformat-dev libswscale-dev libjpeg-dev libpng-dev libtiff-dev libjasper-dev

    COPY nginx /var/opt/bin/nginx-imp/nginx
    COPY libopencv_core.so.2.4 /usr/lib/libopencv_core.so.2.4
    COPY libopencv_imgproc.so.2.4 /usr/lib/libopencv_imgproc.so.2.4
    COPY libopencv_highgui.so.2.4 /usr/lib/libopencv_highgui.so.2.4
    COPY libfreeimage.so.3 /usr/lib/libfreeimage.so.3

    CMD ["/var/opt/bin/nginx-imp/nginx"]
    EXPOSE 80
    ```

4. Build an image with

    ```
    chmod +x build.sh
    ./build.sh
    ```

5. If the build process succeeds, you need to add the `daemon off` directive into /etc/nginx-imp/conf/nginx.conf. It's really important since docker will close container if nginx daemonizes itself.

6. Finally, you can run the container

    ```
    docker run -d \
        -p 127.0.0.10:80:80 \
        -v /etc/nginx-imp/conf:/var/opt/bin/nginx-imp/conf \
        -v /etc/nginx-imp/logs:/var/opt/bin/nginx-imp/logs \
        -v /var/www/imp:/var/www \
        nginx_imp
    ```

    Here we set an explicit IP, mount volumes from a host to the container with a config and logs, and also www folder. Of course, you can change paths as you wish. Remove `-d` key to view emerge nginx alerts if something goes wrong. If no errors are thrown, check `docker ps` to ensure the container is still running. If this is so, you can put any image to the host `/var/www/imp` and try to process it: `curl 127.0.0.10/test.jpg?resize=16` (sorry, this will flood your terminal with unreadable binary data).