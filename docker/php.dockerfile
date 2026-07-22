FROM ubuntu:22.04

ARG PHP_SRC_TYPE
ARG PHP_SRC_DEBUG
ARG PHP_SRC_ASAN
ARG PHP_SRC_TSAN
ARG PHP_SRC_GCOV
ARG PHP_VERSION_MAJOR
ARG PHP_VERSION_MINOR
ARG PHP_VERSION_PATCH
ARG PHP_VERSION_RC

RUN apt-get update

RUN apt-get -y install bash git wget build-essential autoconf pkg-config bison re2c

RUN if test "$PHP_SRC_TSAN" = enable; then \
        wget -qO /etc/apt/trusted.gpg.d/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key && \
        echo "deb https://apt.llvm.org/jammy/ llvm-toolchain-jammy-20 main" > /etc/apt/sources.list.d/llvm.list && \
        apt-get update && \
        apt-get -y install clang-20 llvm-20; \
    fi

RUN mkdir -p /opt/src
RUN mkdir -p /opt/bin
RUN mkdir -p /opt/etc

ADD --chmod=755 docker/php.src /opt/bin

RUN /opt/bin/php.src $PHP_SRC_TYPE $PHP_VERSION_MAJOR $PHP_VERSION_MINOR $PHP_VERSION_PATCH $PHP_VERSION_RC

WORKDIR /opt/src/php-src

RUN ./buildconf --force >/dev/null

RUN if [ "$PHP_SRC_TSAN" = enable ]; then \
        export CC=clang-20 CXX=clang++-20; \
    fi && \
    ./configure --disable-all \
                --disable-cgi \
                --disable-phpdbg \
                --$PHP_SRC_DEBUG-debug \
                --$PHP_SRC_GCOV-gcov \
                --$PHP_SRC_ASAN-address-sanitizer \
                --enable-opcache \
                --enable-zts \
                --prefix=/opt \
                --with-config-file-scan-dir=/opt/etc/php.d \
                --with-config-file-path=/opt/etc

RUN if test "$PHP_SRC_TSAN" = enable; then \
        make -j EXTRA_CFLAGS="-O1 -g -fsanitize=thread -fno-omit-frame-pointer" >/dev/null; \
    else \
        make -j >/dev/null; \
    fi

RUN if test "$PHP_SRC_TSAN" = enable; then \
        make install EXTRA_CFLAGS="-O1 -g -fsanitize=thread -fno-omit-frame-pointer"; \
    else \
        make install; \
    fi

RUN cp php.ini-development /opt/etc/php.ini

RUN mkdir -p /opt/etc/php.d

ENV PATH=/opt/bin:$PATH

RUN if [ "$PHP_SRC_TSAN" != enable ] && [ "$PHP_VERSION_MAJOR" -eq 8 ] && [ "$PHP_VERSION_MINOR" -lt 5 ]; then \
        echo "zend_extension=opcache.so" > /opt/etc/php.d/opcache.ini; \
    fi

RUN test "$PHP_SRC_TSAN" = enable || php -v

RUN test "$PHP_SRC_TSAN" = enable || php --ini

WORKDIR /opt
