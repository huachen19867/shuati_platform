# Ubuntu 公网部署与免费域名

本方案面向一台带公网 IPv4 的 Ubuntu 22.04/24.04 主机。DuckDNS 提供免费的
`*.duckdns.org` 子域名，Nginx 和 Let's Encrypt 提供 HTTPS，提交代码由 Docker
禁网沙箱执行。域名免费不等于服务器免费；如果没有公网主机，需要先准备一台
云主机，或选择带公网端口映射的家用 Linux 机器。

## 1. 准备 DuckDNS 名称

登录 <https://www.duckdns.org/>，创建一个尚未被占用的名称，例如
`my-shuati`，最终域名就是 `my-shuati.duckdns.org`。复制页面上的 token。

主机安全组或防火墙需要放行 TCP 22、80、443。不要把后端 8080 或 Docker
端口暴露到公网。

## 2. 执行安装

```bash
git clone https://github.com/huachen19867/shuati_platform.git
cd shuati_platform
export SHUATI_DOMAIN='my-shuati.duckdns.org'
export SHUATI_DUCKDNS_TOKEN='替换为 DuckDNS token'
export SHUATI_ADMIN_PASSWORD='替换为至少12位的随机密码'
sudo -E bash deploy/install_ubuntu.sh
```

脚本会安装编译依赖、Docker、Nginx 和 Certbot，构建后端及判题镜像，创建
最小权限的 systemd 服务，更新 DuckDNS 解析，签发 HTTPS 证书，并在最后调用
健康检查。全新状态目录会自动创建 `root` 管理员和一道 A+B 入门题。

## 3. 验收与维护

```bash
systemctl status shuati --no-pager
journalctl -u shuati -n 100 --no-pager
curl https://my-shuati.duckdns.org/api/health
docker run --rm shuati-cpp-judge:latest g++ --version
```

浏览器打开域名，注册普通账号后即可进入题目并提交 C++。管理入口使用
`root` 和部署时设置的密码。运行状态位于 `/var/lib/shuati`，配置位于
`/etc/shuati/app.yaml`，日志位于 `/var/log/shuati`。升级代码后重新运行同一
安装脚本即可执行 fast-forward 更新、重编译和重启；状态目录不会被覆盖。

当前持久化层是单机原子状态文件，适合个人和小规模试用，不支持多实例并发。
若要面向大量用户，应再迁移到 MySQL/PostgreSQL，并把同步判题改为独立队列。
