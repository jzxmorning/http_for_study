# jiao_http

C++17 HTTP 服务，包含 MySQL 数据访问、Session 登录和静态网页。

## 本地配置与运行

依赖：g++、make、MySQL 客户端开发库、JsonCpp、Boost 头文件。
测试还需要 Python 3 和 Node.js。

首次克隆后，在项目目录创建本地数据库密码配置：

```bash
cp mysql_password.local.hpp.example mysql_password.local.hpp
chmod 600 mysql_password.local.hpp
```

将 `mysql_password.local.hpp` 中的 `CHANGE_ME` 替换为自己的数据库密码。
该文件被 Git 忽略，不会随代码提交。现有工作目录已保留原来的密码配置。
数据库地址、用户名、库名和端口见 `Mysql.hpp`。

```bash
make
./http
```

从项目目录启动后访问 `http://127.0.0.1:8080/`。

```bash
make test
```

现有测试使用数据库替身，不访问真实数据库。
独立 Session 演示见 [session_demo.md](session_demo.md)。

## 日常 Git 操作

在 `jiao_http` 目录执行：

```bash
# 查看修改
git status
git diff

# 保存一个版本，提交前检查暂存内容
git add .
git diff --cached
git commit -m "描述本次修改"

# 查看版本记录
git log --oneline

# 为新功能创建分支
git switch -c feature/my-change
```

编译产物、日志及本地密码配置均已忽略；源码、网页、测试和文档会纳入管理。
目前仅配置本地版本管理。创建 GitHub 或 Gitee 空仓库后，使用自己的仓库地址：

```bash
git remote add origin <你的仓库地址>
git push -u origin HEAD
```
