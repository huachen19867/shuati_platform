# Shuati Platform

一个轻量级 C++ 在线判题平台。用户可以注册登录、浏览题目、在线编写并提交
C++17 代码、查看逐测试点结果；管理员可以维护题目、测试点、用户角色和提交。

开发环境构建：

```bash
cmake -S . -B build -DSHUATI_BUILD_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/shuati_server --config config/app.yaml
```

本地配置默认使用宿主机 `g++`，仅用于开发。任何公网部署都必须使用
`judge.runner: docker`。完整的免费域名与 HTTPS 部署步骤见
[Ubuntu 公网部署与免费域名](docs/deployment/free-domain-ubuntu.md)。
