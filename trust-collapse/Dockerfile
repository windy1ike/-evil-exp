FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    COMPOSER_HOME=/tmp/composer

RUN apt-get update && apt-get install -y --no-install-recommends \
    nginx \
    php8.2-fpm \
    php8.2-cli \
    php8.2-curl \
    php8.2-mbstring \
    php8.2-xml \
    php8.2-zip \
    php8.2-sqlite3 \
    python3 \
    python3-pip \
    curl \
    ca-certificates \
    unzip \
    git \
    supervisor \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN curl -sS https://getcomposer.org/download/2.9.5/composer.phar -o /usr/local/bin/composer \
    && chmod +x /usr/local/bin/composer

# Python deps for microservices
RUN pip3 install --break-system-packages requests 2>/dev/null || true

# PHP-FPM config: cgi.fix_pathinfo=0 (prevents path injection)
RUN sed -i 's/;cgi.fix_pathinfo=1/cgi.fix_pathinfo=0/' /etc/php/8.2/fpm/php.ini

# Create app directories
RUN mkdir -p /var/www/html/public \
    /opt/services/coordinator \
    /opt/services/builder \
    /var/log/supervisor \
    /run/php \
    /var/lib/php/sessions \
    && chown -R www-data:www-data /var/www/html

COPY web/composer.json /var/www/html/composer.json
WORKDIR /var/www/html
RUN composer config --global audit.block-insecure false \
    && composer install --no-interaction --no-progress

# Copy web application
COPY web/public/ /var/www/html/public/

# Remove default nginx config, use conf.d approach (works on debian slim)
RUN rm -f /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default
COPY nginx/default.conf /etc/nginx/conf.d/default.conf

# Copy services
COPY coordinator/server.py /opt/services/coordinator/server.py
COPY builder/server.py /opt/services/builder/server.py

# Copy supervisor config
COPY supervisor/supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# Entrypoint
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

USER root

EXPOSE 80
ENTRYPOINT ["/entrypoint.sh"]
