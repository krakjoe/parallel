Requirements
============

  * PHP 8.0
  * ZTS
  * <pthread.h>

Installation
============

**From PECL (recommended)**

```bash
pecl install parallel
```

**Binary distributions**

Microsoft Windows binaries are released through [PECL](https://pecl.php.net/package/parallel).

**From sources**

```bash
git clone https://github.com/krakjoe/parallel.git
cd parallel
phpize
./configure --enable-parallel  [ --enable-parallel-coverage ] [ --enable-parallel-dev ]
make
make test
make install
```

> [!NOTE]  
> This will install the latest version from the `develop` branch and should be
> considered unstable!
