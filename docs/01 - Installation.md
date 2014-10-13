#Installation

##Prerequisites
All tips in this doc related to Ubuntu 12.04. If you use less popular OS, it's up to you do all this stuff below.

You will need **git**. If you don't have git, just install it with packet manager

    sudo apt-get install git

----------

##Install OpenCV from sources

We will need openCV. Offsite instructions is build it from source.

    sudo apt-get update
    sudo apt-get upgrade
    sudo apt-get install build-essential cmake libgtk2.0-dev python-dev python-numpy libavcodec-dev libavformat-dev libswscale-dev libjpeg-dev libpng-dev libtiff-dev libjasper-dev webp
        
Download sources. You can do it with wget, but source stored on SourceForge which has it's own handler for downloads. Much easier to download it with your favorite browser and put to server via sftp client. Get your copy [here](http://sourceforge.net/projects/opencvlibrary/files/opencv-unix/2.4.9/opencv-2.4.9.zip/download). I put mine into /var/opt/src/OpenCV.
    
   It's compile time.
    
    cd /var/opt/src/OpenCV
    cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local .
    
If configure was ok, run
    
    make
    sudo make install
    
This will take... a while.

**[Optional]**. Probably your ldconfig already know about `/usr/local/lib`, but if you get message that dynamic library is not found, put 
    
    /usr/local/lib
    
to /etc/ld.so.conf and update binding:
    
    sudo ldconfig

----------

##Get nginx sources
    
    cd ..
    git clone https://github.com/nginx/nginx.git

**[Optional].** You can download `pcre`, `openssh` and/or `zlib`. It used for some nginx features, but it not necessary if you build IMP-enabled nginx to separated service. As for me, I downloaded `pcre`.

----------

##Get IMP sources

    git clone https://github.com/tommiv/ngx_http_imgproc.git

----------

##It's build time

I assume you still in `/var/opt/src` and you have all needed sources here: nginx, optional pcre|libssh|zlib and IMP.
        
    cd nginx
    
Write configure line for nginx
        
    ./configure \
        --prefix=/var/opt/bin/nginx-imp \
        --sbin-path=./ \
        --conf-path=./conf/nginx.conf \
        --pid-path=./nginx.pid \
        --without-http_rewrite_module \
        --add-module=../ngx_http_imgproc
    
You can just copy/paste it if `/var/opt/bin/nginx-imp` binary path is ok for you. If you downloaded any additional libs like zlib, you can add it to configure command, as described [here](http://wiki.nginx.org/InstallOptions).

Wait until configure is finished. If you don't see "error" word, all is ok.
    
Now is your chance. Type

    make
    
If you lucky enough, you'll see compiler message about all is complete. Check for it:
        
    objs/nginx
    
You'll see message about not found log and config files. It's ok, proceed to configuration step. 
    
If you see "not found" message, compilation was unsuccessful. 
    
If you see `libdc1394 error: Failed to initialize libdc1394` – don't worry too much. This is not an error actually, it's warning. I wasn't able to found easy fix for this, but it have no any consequences, it's just annoying.

----------

##Put binary where it belong

Make dirs. Copy nginx binary and config stub to working directory

    mkdir /var/opt/bin/nginx-imp
    mkdir /var/opt/bin/nginx-imp/conf
    mkdir /var/opt/bin/nginx-imp/logs
    cp objs/nginx /var/opt/bin/nginx-imp/
    cp conf/* /var/opt/bin/nginx-imp/conf
    
We're done here. You can go to destination folder

    cd /var/opt/bin/nginx-imp

and start to configure service.

----------
##[Optional] Add IMP to upstart

Based on <s>actual events</s> [this article](http://articles.slicehost.com/2007/10/17/ubuntu-lts-adding-an-nginx-init-script).

If you used same binary path as me, just put `/docs/nginx-imp` script to `/etc/init.d/`. Otherwise open it first and modify DAEMON and PIDFILE variables on lines 15-16 to your own.

Make script executable

    sudo chmod +x /etc/init.d/nginx-imp
    
Update run levels

    sudo /usr/sbin/update-rc.d -f nginx-imp defaults
    
That's all, now you can use

    service nginx-imp start
    service nginx-imp stop
    service nginx-imp reload
    
to control IMP.

If you don't want to this crazy stuff, you can run IMP with

    /var/opt/bin/nginx-imp/nginx
    
And stop/restart with signal send

    /var/opt/bin/nginx-imp/nginx -s %signal%
    i.e.
    /var/opt/bin/nginx-imp/nginx -s stop

----------

##Update/rebuild

Pull new nginx and/or IMP sources

    cd /var/opt/src/nginx
    git pull origin master
    
    cd /var/opt/src/ngx_http_imgproc
    git pull origin master
    
Run make inside 
    
    cd /var/opt/src/nginx
    make

If you'll see errors, try to re-run `./configure` as described in **It's build time** section and run `make` again.

Replace binary
    
    service nginx-imp stop
    cp objs/nginx /var/opt/bin/nginx-imp/
    service nginx-imp start