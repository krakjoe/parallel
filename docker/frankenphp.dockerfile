FROM dunglas/frankenphp:1.12.7-php8.5-bookworm AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends $PHPIZE_DEPS

COPY config.m4 Makefile.frag php_parallel.c php_parallel.h /usr/src/parallel/
COPY src/*.c src/*.h /usr/src/parallel/src/
WORKDIR /usr/src/parallel
RUN phpize \
 && ./configure --enable-parallel \
 && make -j"$(nproc)" \
 && make install

FROM dunglas/frankenphp:1.12.7-php8.5-bookworm
COPY --from=build /usr/local/lib/php/extensions/ /usr/local/lib/php/extensions/
RUN docker-php-ext-enable parallel
