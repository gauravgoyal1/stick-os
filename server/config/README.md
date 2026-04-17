# Server Config

Production config files for the Stick OS server. Replace `$STICK_DOMAIN` with your actual domain before use.

## Nginx

Copy and replace the domain placeholder:

```bash
export STICK_DOMAIN="your.domain.com"
envsubst '$STICK_DOMAIN' < config/nginx.conf > /etc/nginx/sites-available/$STICK_DOMAIN
ln -s /etc/nginx/sites-available/$STICK_DOMAIN /etc/nginx/sites-enabled/
nginx -t && systemctl reload nginx
```

SSL certs via Certbot:

```bash
certbot --nginx -d $STICK_DOMAIN
```

## Systemd

Install as a user service. Replace `$STICK_OS_SERVER_DIR` with the absolute path to `server/`:

```bash
export STICK_OS_SERVER_DIR="/path/to/stick-os/server"
envsubst '$STICK_OS_SERVER_DIR' < config/stick-os-server.service > ~/.config/systemd/user/stick-os-server.service
systemctl --user daemon-reload
systemctl --user enable --now stick-os-server
```

Manage:

```bash
systemctl --user status stick-os-server
systemctl --user restart stick-os-server
journalctl --user -u stick-os-server -f
```
