#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "请使用 sudo 运行：sudo -E bash deploy/install_ubuntu.sh" >&2
  exit 1
fi

: "${SHUATI_DOMAIN:?请设置完整域名，例如 oj-demo.duckdns.org}"
: "${SHUATI_DUCKDNS_TOKEN:?请设置 DuckDNS token}"
: "${SHUATI_ADMIN_PASSWORD:?请设置至少 12 位管理员密码}"

if [[ ! ${SHUATI_DOMAIN} =~ ^[a-z0-9][a-z0-9-]{0,62}\.duckdns\.org$ ]]; then
  echo "当前脚本只接受 *.duckdns.org 免费域名" >&2
  exit 1
fi
if [[ ! ${SHUATI_ADMIN_PASSWORD} =~ ^[A-Za-z0-9._~!@#%+=:-]{12,128}$ ]]; then
  echo "管理员密码须为 12-128 位，且不能含引号、反斜杠或空格" >&2
  exit 1
fi

domain_prefix=${SHUATI_DOMAIN%.duckdns.org}
repo_url=${SHUATI_REPO_URL:-https://github.com/huachen19867/shuati_platform.git}
install_dir=/opt/shuati-platform

apt-get update
apt-get install -y --no-install-recommends \
  build-essential cmake git curl ca-certificates libssl-dev libcrypt-dev \
  docker.io nginx certbot python3-certbot-nginx

systemctl enable --now docker nginx
if ! id shuati >/dev/null 2>&1; then
  useradd --system --home /var/lib/shuati --create-home --shell /usr/sbin/nologin shuati
fi
usermod -aG docker shuati

if [[ -d ${install_dir}/.git ]]; then
  git -C "${install_dir}" pull --ff-only
else
  git clone --depth 1 "${repo_url}" "${install_dir}"
fi

cmake -S "${install_dir}" -B "${install_dir}/build/production" \
  -DSHUATI_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "${install_dir}/build/production" --parallel "$(nproc)"
docker build -t shuati-cpp-judge:latest "${install_dir}/docker/cpp"

install -d -m 0750 -o shuati -g shuati \
  /etc/shuati /var/lib/shuati/state /var/lib/shuati/testcases \
  /var/lib/shuati/submissions /var/lib/shuati/judge_tmp /var/log/shuati

cat > /etc/shuati/app.yaml <<EOF
app:
  name: "shuati-platform"
  environment: "production"
server:
  host: "127.0.0.1"
  port: 8080
  public_dir: "${install_dir}/public"
judge:
  runner: "docker"
  docker_binary: "/usr/bin/docker"
  docker_image: "shuati-cpp-judge:latest"
  workers: 2
  source_size_limit_kb: 64
  compile_timeout_ms: 10000
  run_timeout_ms: 2000
  memory_limit_mb: 128
  output_limit_kb: 1024
  compile_message_limit_kb: 8
  stderr_limit_kb: 4
  temp_dir: "/var/lib/shuati/judge_tmp"
storage:
  testcase_dir: "/var/lib/shuati/testcases"
  submission_dir: "/var/lib/shuati/submissions"
  source_retention_hours: 24
  state_dir: "/var/lib/shuati/state"
logs:
  level: "info"
  access: "/var/log/shuati/access.log"
  error: "/var/log/shuati/error.log"
  judge: "/var/log/shuati/judge.log"
security:
  session_ttl_hours: 168
  submit_interval_seconds: 5
  upload_max_mb: 20
bootstrap:
  super_admin:
    enabled: true
    username: "root"
    password: "${SHUATI_ADMIN_PASSWORD}"
EOF
chmod 0640 /etc/shuati/app.yaml
chown root:shuati /etc/shuati/app.yaml

install -m 0644 "${install_dir}/deploy/shuati.service" /etc/systemd/system/shuati.service
systemctl daemon-reload
systemctl enable --now shuati

cat > /etc/nginx/sites-available/shuati <<EOF
server {
    listen 80;
    listen [::]:80;
    server_name ${SHUATI_DOMAIN};
    client_max_body_size 20m;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_read_timeout 300s;
    }

    add_header X-Content-Type-Options nosniff always;
    add_header Referrer-Policy strict-origin-when-cross-origin always;
    add_header X-Frame-Options DENY always;
}
EOF
ln -sfn /etc/nginx/sites-available/shuati /etc/nginx/sites-enabled/shuati
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl reload nginx

install -d -m 0700 /etc/duckdns
cat > /etc/duckdns/shuati.env <<EOF
DUCKDNS_DOMAIN=${domain_prefix}
DUCKDNS_TOKEN=${SHUATI_DUCKDNS_TOKEN}
EOF
chmod 0600 /etc/duckdns/shuati.env
cat > /usr/local/sbin/update-shuati-duckdns <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
source /etc/duckdns/shuati.env
result=$(curl --fail --silent --show-error \
  "https://www.duckdns.org/update?domains=${DUCKDNS_DOMAIN}&token=${DUCKDNS_TOKEN}&ip=")
[[ ${result} == "OK" ]]
EOF
chmod 0750 /usr/local/sbin/update-shuati-duckdns
/usr/local/sbin/update-shuati-duckdns
cat > /etc/cron.d/shuati-duckdns <<'EOF'
*/5 * * * * root /usr/local/sbin/update-shuati-duckdns >/dev/null 2>&1
EOF

certbot --nginx --non-interactive --agree-tos --redirect \
  --register-unsafely-without-email -d "${SHUATI_DOMAIN}"

curl --fail --silent --show-error "https://${SHUATI_DOMAIN}/api/health" >/dev/null
echo "部署完成：https://${SHUATI_DOMAIN}/"
