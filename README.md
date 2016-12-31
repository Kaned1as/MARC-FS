  MARC-FS
===========
Mail.ru Cloud filesystem written for FUSE

Synopsis
--------
This is an implementation of a simple filesystem with all calls and hooks needed for normal file operations. After mounting it you'll be provided access to all your cloud files remotely stored on Mail.ru Cloud as if they were local ones. You should keep in mind that this is a network-driven FS and so it will never be as fast as any local one, but having a folder connected as remote drive in 9P/GNU Hurd fashion can be convenient at a times.

Installation
------------
You should have cmake and g++ at hand.
MARC-FS also requires `libfuse` (obviously), `libcurl` and `pthread` libraries. Once you have all this, do as usual:


    $ cd MarcFS
    $ mkdir build
    $ cd build && cmake ..
    $ make
    $ # here goes the step where you actually go and register on mail.ru website to obtain cloud storage and auth info
    $ ./marcfs /path/to/empty/folder -o username=your.email@mail.ru,password=your.password

API references
--------------
- There is no official Mail.ru Cloud API reference, everything is reverse-engineered. You may refer to [Doxygen API comments](https://gitlab.com/Kanedias/MailFuse/blob/master/marcfs_api.h) to grasp concept of what's going on.
- FUSE: [API overview](https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201109/homework/fuse/fuse_doc.html) - used to implement FS calls
- cURL: [API overview](https://curl.haxx.se/docs/) - used to interact with Mail.ru Cloud REST API

Motivation
----------
Mail.ru is one of largest Russian social networks. It provides mail services, hosting, gaming platforms and, incidentally, cloud services, similar to Dropbox, NextCloud etc.

Once upon a time Mail.ru did a discount for this cloud solution and provided beta testers (and your humble servant among them) with free 1 TiB storage.

And so... A holy place is never empty.

Bugs & Known issues
-------------------
1. Temporary
  - No multithreading at the moment
  - No tests :(
  - No support for files larger than 2GB (seriously dunno what would happen)
2. Principal (Mail.ru Cloud API limitations)
  - No statfs support, you cannot determine how much place is left on cloud storage
  - No extended attr/chmod support, all files on storage are owned by you
  - No atime/ctime support, only mtime is stored
  - No `Transfer-Encoding: chunked` support for POST **requests** in cloud nginx (`chunkin on`/`proxy_request_buffering` options in `nginx`/`tengine` config), so files are read fully into memory before uploading

Contributions
------------
You may create merge request or bug/enhancement issue right here on GitLab, or send foramtted patch via e-mail. Audits from code style and security standpoint are also much appreciated.

License
-------

    Copyright (C) 2016-2017  Oleg `Kanedias` Chernovskiy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
