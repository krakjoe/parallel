ARG PHP_SRC_TYPE
ARG PHP_SRC_DEBUG
ARG PHP_VERSION_MAJOR
ARG PHP_VERSION_MINOR
ARG PHP_VERSION_PATCH

FROM php-$PHP_SRC_TYPE-$PHP_VERSION_MAJOR.$PHP_VERSION_MINOR.$PHP_VERSION_PATCH:parallel

ADD . /opt/parallel

RUN php -v

RUN phpize --clean >/dev/null

RUN phpize >/dev/null

RUN ./configure --enable-parallel >/dev/null

RUN make -j >/dev/null

RUN make install >/dev/null

RUN echo "extension=parallel.so" > /etc/php.d/parallel.ini

RUN php -m