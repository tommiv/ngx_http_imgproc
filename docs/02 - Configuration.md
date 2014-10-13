#Configuration

##Preflight

You have a choice: edit config in console
    
    cd /var/opt/bin/nginx-imp
    nano conf/nginx.conf

Or just upload new one via sftp.

Here is minimal config:

    user www-data;
    worker_processes 2; # up to you to configure
    
    events {
        worker_connections  1024; # same here
    }

    http {
        include           mime.types;
        default_type      application/octet-stream;
        sendfile          on;
        keepalive_timeout 5;
    
        server {
            listen      9123; # I believe your 80 already used
            server_name imp.pixl.farm; # replace it with your hostname or leave "localhost"
    
            location /process/ {
                imgproc on;
                alias /var/www/imp-demo/; # here is your actual images folder
                # allow 127.0.0.0/8;
                # deny all;
            }
        }
    }

As you can see we exposed it to internet directly. Check if all is ok: put some images to images folder. Play with query arguments and config options. See below to find available config options and /docs/Usage.md for query arguments. When all is done, return here.

**[Note]**. We will deal with two instances of nginx: **main** (stable) – the one that really serves your site, and **IMP** – the one that process pics. I will use this markers below to show you which config you should edit.

----------

##Hide IMP in local network

Most likely you don't want to allow all the world to asking your images processor server. And of course you want to enable cache of processed images. Change 

    server_name  localhost; 
        
in **IMP** config and uncomment last two directives in location config

    allow 127.0.0.0/8;
    deny all;
        
    Reload IMP config with 
        
    service nginx-imp reload
    
You can't open IMP from internet anymore.

----------

##Set up proxy

Now it's time to set up proxy from your main HTTP server to IMP. Open your **main** nginx installation config, wherever you put it.

Add proxy cache directive inside *http* section

    proxy_cache_path 
        /var/www/imp-demo/cache #the line you probably will change
        levels=1:2 
        keys_zone=imgproc:10m 
        inactive=24h 
        max_size=5G;

Add new location in *server* section. This sample will serve http://yoursite/img/

    location /img/ {
        if ($uri ~* "^/img/(.*)" ) {
            set $filename $1;
        }
        proxy_pass        http://127.0.0.1:9123/process/$filename?$args;
        proxy_cache       imgproc;
        proxy_cache_valid 200     24h;
        proxy_cache_valid 404 415 5s;
        proxy_set_header  Host $host;
    }
    
I believe it's intuitive enough. If something goes wrong, first check `proxy_pass` address and `proxy_cache` name. Also nginx log is your best friend. That's all – now your frontend should pass requests to /img/%filename%.jpg to IMP and cache result.

----------

##Config options

###imgproc
    Values: on/off
    Default: off
    Allowed at: location
    
Enables/Disables module in particular location.

    imgproc on;

###imgproc_watermark
    Values: 1 (path to image)
    Default: disabled
    Allowed at: location, server
    
If presented, adds overlay (watermark) to all images in location/server. If not presented or path is invalid, do nothing.

    imgproc_watermark /var/www/imp-demo/watermark.png;
    
###imgproc_watermark_opacity
    Values: 1 (int from 0 to 100)
    Default: 100 (totally opaque)
    Allowed at: location, server
    
Does exactly what it says.
    
    imgproc_watermark_opacity 40;

![](http://i.imgur.com/I1mlwPx.jpg)
    
###imgproc_watermark_opacity
    Values: 4 (GravityX: enum, GravityY: enum, OffsetX: int, OffsetY: int)
    Default: not applicable
    Allowed at: location, server

Defines overlay position. Argument 1 is x gravity, can be l/c/r (left, center, right). Argument 2 is y gravity, can be t/c/b (top, center, bottom). Args 3-4 is offset from gravity in pixels.
    
    # stick overlay to center-top and offset to 20px down
    imgproc_watermark_position c t 0 20;

![](http://i.imgur.com/sV5rzBg.jpg)
    
###imgproc_max_src_size

    Values: 1 (nginx parseable size)
    Default: 4M
    Allowed at: location, server
    
Watchdog for max **source** image size. IMP will throw 415 Unsupported media, if size exceeded limit.

    # denies processing of images bigger than 1Mb
    imgproc_max_src_size 1M;
    
###imgproc_max_target_dimensions

    Values: 2 (int)
    Default: 2000 2000
    Allowed at: location, server
    
Watchdogs for max **target** dimensions, in pixels. IMP will throw 415 Unsupported media, if size exceeded limit.

    # denies resize to target bigger than FullHD
    imgproc_max_target_dimensions 1920 1080;
    
###imgproc_max_filters_count

    Values: 1 (int)
    Default: 5
    Allowed at: location, server
    
Watchdog for max number of simultaneous filters per request. Use 0 to disabled filters.
    
    # still allow you to do modulation and flip at the same time
    imgproc_max_filters_count 2;
    
###imgproc_allow_experiments

    Values: 1 (on/off)
    Default: off
    Allowed at: location, server

Allows unstable or slow filters marked as experimental. See `/docs/Usage.md` section for list.

    # now you can use vignetting
    imgproc_allow_experiments on;

----------

##[Optional]. Templating

We can use nginx config abilities to create query templates.

Put this demo map to your **main** http section

    map $args $img_template {
        "~*template=default" "crop=3,2,c,t";
        "~*template=square"  "crop=1,1,c,c";
    }
    
Create /tmpl/ location in server section

    location /tmpl/ {
        if ($uri ~* "^/tmpl/(.*)" ) {
            set $filename $1;
        }
        set $req $args;
        if ($img_template) {
            set $req $req&$img_template;
        }
        proxy_pass        http://127.0.0.1:9123/process/$filename?$req;
        proxy_cache       imgproc;
        proxy_cache_valid 200     24h;
        proxy_cache_valid 404 415 5s;
        proxy_set_header  Host $host;
    }

What's going on here? We created map, that will put crop request to `$img_template` if original query contains `template` argument. In location we check this variable, and in case we have map hit, we appending crop to the end of original request. IMP will use last crop entry while `template` argument itself will be ignored. Therefore,

    /tmpl/myphoto.jpg?template=square&filter-blur=1
    translates into
    %proxy%/process/myphoto.jpg?filter-blur=1&crop=1,1,c,c #and template=square part, which will be ignored