# Tech info

## HTTP codes

**400 Bad Request**

- Invalid argument(s) - wrong arguments count, invalid value

**405 Method Not Allowed**

- Unknown filter requested
- Filters limit exceeded

**413 Request Entity Too Large**

- Requested output image size is above limits

**415 Unsupported Media Type**

- Image cannot be decoded
- Source image is too large

**424 Failed Dependency**

- Requested an operation which is not supported (occurs when feature was not enabled during compilation; for now this is WEBP support only)

**500 Internal Server Error**

- Something went wrong. See **debug** topic below

***

## Debug

Since nginx is a server software, it's not very easy to debug it. Nginx provides an interface for errors logging, and you can add the `--with-debug` option to the configuration script.

IMP has the IMP_DEBUG macro. It is disabled by default, but you can uncomment it in `required.h`. This will enable some additional messages and writing into syslog. If something has failed, you will see an error like this:

```
imp::Job failed at step %d with code %d
```

Step codes, as well as error codes, can be found in `required.h`.