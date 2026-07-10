ARG PHP_SRC_TYPE
ARG PHP_VERSION
ARG PHP_IMAGE_TAG=latest

FROM ghcr.io/krakjoe/php-$PHP_SRC_TYPE-$PHP_VERSION:$PHP_IMAGE_TAG

ARG PHP_SRC_TYPE

RUN test $PHP_SRC_TYPE != "gcov" || apt-get -y install lcov

ADD . /opt/parallel

WORKDIR /opt/parallel

RUN php -v

RUN phpize --clean

RUN phpize >/dev/null

RUN mkdir -p /opt/build/parallel

WORKDIR /opt/build/parallel

RUN /opt/parallel/configure --enable-parallel \
    --$(test "$PHP_SRC_TYPE" = asan && echo enable || echo disable)-parallel-address-sanitizer \
    --$(test "$PHP_SRC_TYPE" = gcov && echo enable || echo disable)-parallel-gcov >/dev/null

RUN make -j >/dev/null

RUN make install >/dev/null

RUN echo "extension=parallel.so" > \
        /opt/etc/php.d/parallel.ini

RUN php -m

WORKDIR /opt/parallel
