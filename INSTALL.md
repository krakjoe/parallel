Requirements
============

  * PHP >= 8.0
  * ZTS
  * <pthread.h>

Installation
============

**From PIE**

Asuming you have [`pie`](https://github.com/php/pie) installed.

```bash
pie install pecl/parallel
```

**Binary distributions**

Microsoft Windows binaries are attached to the [releases on GitHub](https://github.com/krakjoe/parallel/releases).

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

**From PECL**

```bash
pecl install parallel
```

> [!NOTE]
> PECL is deprecated, please use PIE instead.
