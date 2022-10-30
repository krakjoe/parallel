ARG PHP_SRC_TYPE
ARG PHP_VERSION_MAJOR
ARG PHP_VERSION_MINOR

FROM php-$PHP_SRC_TYPE:$PHP_VERSION_MAJOR.$PHP_VERSION_MINOR

ARG PHP_SRC_TYPE

RUN test $PHP_SRC_TYPE != "gcov" || apt-get -y install lcov

ADD . /opt/parallel

WORKDIR /opt/parallel

RUN php -v

RUN phpize --clean

RUN phpize >/dev/null

RUN mkdir -p /opt/build/parallel

WORKDIR /opt/build/parallel

ARG PHP_SRC_ASAN
ARG PHP_SRC_GCOV

RUN /opt/parallel/configure --enable-parallel --$PHP_SRC_ASAN-parallel-address-sanitizer --$PHP_SRC_GCOV-parallel-gcov >/dev/null

RUN make -j >/dev/null

RUN make install >/dev/null

RUN echo "extension=parallel.so" > \
        /opt/etc/php.d/parallel.ini

RUN php -m

WORKDIR /opt/parallel