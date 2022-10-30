ARG UBUNTU_VERSION_MAJOR
ARG UBUNTU_VERSION_MINOR

FROM ubuntu:$UBUNTU_VERSION_MAJOR.$UBUNTU_VERSION_MINOR

ARG PHP_SRC_TYPE
ARG PHP_SRC_DEBUG
ARG PHP_SRC_ASAN
ARG PHP_SRC_GCOV
ARG PHP_VERSION_MAJOR
ARG PHP_VERSION_MINOR
ARG PHP_VERSION_PATCH
ARG PHP_VERSION_RC

RUN apt-get update

RUN apt-get -y install bash git wget build-essential autoconf pkg-config bison re2c

RUN mkdir -p /opt/src
RUN mkdir -p /opt/bin
RUN mkdir -p /opt/etc

ADD docker/php.src /opt/bin

RUN /opt/bin/php.src $PHP_SRC_TYPE $PHP_VERSION_MAJOR $PHP_VERSION_MINOR $PHP_VERSION_PATCH $PHP_VERSION_RC

WORKDIR /opt/src/php-src

RUN ./buildconf --force >/dev/null

RUN ./configure --disable-all \
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

RUN make -j >/dev/null

RUN make install

RUN cp php.ini-development /opt/etc/php.ini

RUN mkdir -p /opt/etc/php.d

ENV PATH=/opt/bin:$PATH

RUN echo "zend_extension=opcache.so" > /opt/etc/php.d/opcache.ini

RUN php -v

RUN php --ini

WORKDIR /opt