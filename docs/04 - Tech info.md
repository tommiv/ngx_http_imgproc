#Tech info

##HTTP codes

**400 Bad Request**. Returns when

- Method arguments format is wrong (type, count or out of range)

**405 Method Not Allowed**. Returns when

- Unknown filter requested;
- Filters limit exceeded;

**413 Request Entity Too Large**. Returns when

- Target is bigger than limits;

**415 Unsupported Media Type**. Returns when

- Image cannot be decoded;
- Source is bigger than limit;

**424 Failed Dependency**. Returns when

- Requested operation which not supported because disabled on compilation level (for now this is WEBP support only)

**500 Internal Server Error**. Returns when 

- something went wrong. See **debug** topic.

----------

##Debug

Since nginx is server software, it can be hard to debug it. Nginx provides interface for log errors, and you can additionally add `--with-debug` option to configure script. 

IMP has IMP_DEBUG macro. It disabled by default, but you can uncomment it in `required.h`. This will enable some additional messages and also enable writes to syslog. If job was not successfull, you should see error like

    imp::Job failed at step %d with code %d
    
Step codes, as well as error codes you can find in `required.h`.