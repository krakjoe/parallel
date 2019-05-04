Requirements
============

  * PHP 7.2+
  * ZTS
  * <pthread.h>

Installation
============

**From sources**

    git clone https://github.com/krakjoe/parallel.git
    cd parallel
    phpize
    ./configure --enable-parallel  [ --enable-parallel-coverage ] [ --enable-parallel-dev ]
    make
    make test
    make install

**From PECL**

    pecl install parallel-beta

**Binary distributions**

  * **Microsoft Windows**: use zip archive from [https://windows.php.net/downloads/pecl/releases/parallel/](https://windows.php.net/downloads/pecl/releases/parallel/)



