##Fork of the official pkgin git repository made to build on Darwin.

Tested and working on Darwin 13 ( Mac OS X 10.9 Mavericks )

Pkgsrc Dependencies:

- bmake 
- sqlite3
- libnbcompat
- libfetch
- libarchive
- pkg_install

Configure:

`$ ./configure --prefix=/usr/pkg --with-libraries=/usr/pkg/lib --with-includes=/usr/pkg/include`

Compile:

`$ bmake`

Install:

`$ sudo bmake install`

Contact me if the build fails at youri.mout@gmail.com!




==================================
Please refer to http://pkgin.net/

Emile "iMil" Heitor <imil@NetBSD.org>
