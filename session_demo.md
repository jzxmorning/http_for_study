# Session 代码演示

`session_demo.cc` 独立复用现有 HTTP 服务器，不连接 MySQL，不修改正式服务。
演示账号是 `student`，密码是 `123456`。默认端口为 `8081`。

## 编译并运行

在 `jiao_http` 目录执行：

```bash
g++ -std=c++17 -pthread session_demo.cc -ljsoncpp -o session_demo
./session_demo
```

另开一个终端，按顺序执行下面的请求。`-c` 让 curl 保存服务器发来的 Cookie，
`-b` 让 curl 在后续请求中携带 Cookie。浏览器同源请求会自动完成这两件事。

```bash
# 1. 尚未登录：返回 HTTP 401。
curl -i http://127.0.0.1:8081/api/me

# 2. 登录：返回 HTTP 200，并通过 Set-Cookie 发放随机票据。
curl -i -c /tmp/jiao-session-demo.cookies \
  -H 'Content-Type: application/json' \
  --data '{"idcount":"student","idpasswd":"123456"}' \
  http://127.0.0.1:8081/api/login

# 3. 携带票据：返回 HTTP 200 和用户编号 1001。
curl -i -b /tmp/jiao-session-demo.cookies \
  http://127.0.0.1:8081/api/me

# 4. 退出：服务器删除 Session，浏览器清除 Cookie。
curl -i -b /tmp/jiao-session-demo.cookies -c /tmp/jiao-session-demo.cookies \
  -X POST http://127.0.0.1:8081/api/logout

# 5. 再次访问：返回 HTTP 401。
curl -i -b /tmp/jiao-session-demo.cookies \
  http://127.0.0.1:8081/api/me
```

## 按这个顺序读代码

1. `Session` 和 `sessions`：服务器记录“随机票据对应哪个用户”。
2. `HandleLogin()`：验证演示账号，创建记录，通过 `Set-Cookie` 发票据。
3. `GetSessionId()` 和 `HandleMe()`：从请求的 `Cookie` 找到用户。
4. `HandleLogout()`：删除记录，让旧票据无法继续使用。

Session 有效期是 30 分钟，保存在内存中，重启示例会清空。
Cookie 中只存随机票据；用户编号和账号保存在服务器，密码不存入 Session。

接入正式项目时，将演示账号判断替换成现有的 `mysql->LogIn(...)`，
再用查到的 `u.id`、`u.idcount` 调用 `CreateSession()`。
判题接口也可以先查 Session，取得当前用户编号，再保存答题记录。
