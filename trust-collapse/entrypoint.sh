#!/bin/bash
set -e

echo "[BuildGate] Starting services..."

# Ensure required directories exist
mkdir -p /run/php /var/log/supervisor /tmp

# Set proper permissions
chown -R www-data:www-data /var/www/html

if [ -z "$BUILD_SIGN_KEY" ]; then
    BUILD_SIGN_KEY="$(python3 - <<'PY'
import secrets
print(secrets.token_urlsafe(24))
PY
)"
    export BUILD_SIGN_KEY
fi

if [ -z "$FLAG" ]; then
    FLAG='SpiritGame{demo_flag}'
    export FLAG
fi

if [ -z "$ADMIN_PASSWORD" ]; then
    ADMIN_PASSWORD="$(python3 - <<'PY'
import secrets
print('Adm_' + secrets.token_urlsafe(14))
PY
)"
    export ADMIN_PASSWORD
fi

# Verify critical binaries
echo "[BuildGate] Composer version: $(composer --version 2>/dev/null || echo 'not found')"
echo "[BuildGate] PHP version: $(php -v 2>/dev/null | head -1 || echo 'not found')"
echo "[BuildGate] Admin password: ${ADMIN_PASSWORD}"

# Start all services via supervisord
echo "[BuildGate] Launching supervisor..."
exec /usr/bin/supervisord -n -c /etc/supervisor/conf.d/supervisord.conf
