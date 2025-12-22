#!/bin/bash

# Script to conditionally configure opcache based on PHP version
# In PHP 8.5+, opcache is built-in and doesn't need to be loaded as an extension

PHP_VERSION_MAJOR=$1
PHP_VERSION_MINOR=$2

# Compare version: if PHP < 8.5, load opcache as extension
if [ "$PHP_VERSION_MAJOR" -lt 8 ] || ([ "$PHP_VERSION_MAJOR" -eq 8 ] && [ "$PHP_VERSION_MINOR" -lt 5 ]); then
    echo "PHP ${PHP_VERSION_MAJOR}.${PHP_VERSION_MINOR}: Loading opcache as zend_extension"
    echo "zend_extension=opcache.so" > /opt/etc/php.d/opcache.ini
else
    echo "PHP ${PHP_VERSION_MAJOR}.${PHP_VERSION_MINOR}: Opcache is built-in, not loading as extension"
    # Create empty file to avoid any issues with missing config
    touch /opt/etc/php.d/opcache.ini
fi
