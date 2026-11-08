// 独立运行：g++ -std=c++17 -pthread session_demo.cc -ljsoncpp -o session_demo
// 演示账号：student，密码：123456。此程序不连接数据库。
#include "http.hpp"
#include "Mutex.hpp"
#include <jsoncpp/json/json.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <sys/random.h>

namespace SessionDemo
{
using Clock = std::chrono::steady_clock;
constexpr int kLifetimeSeconds = 30 * 60;
constexpr const char *kCookieName = "demo_session_id";

// 1. 服务器保存的登录信息，不包含密码。
struct Session
{
    int user_id = 0;
    std::string idcount;
    Clock::time_point expires_at;
};

// 放在函数外，处理完一次请求后仍然存在。
std::unordered_map<std::string, Session> sessions;
MutexModule::Mutex sessions_mutex;

void Reply(HttpResponse *rsp, int status, const Json::Value &body)
{
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    rsp->_statu = status;
    rsp->SetHeader("Cache-Control", "no-store");
    rsp->SetContent(Json::writeString(writer, body), "application/json; charset=utf-8");
}

void ReplyError(HttpResponse *rsp, int status, const std::string &message)
{
    Json::Value body(Json::objectValue);
    body["success"] = false;
    body["message"] = message;
    Reply(rsp, status, body);
}

// 用 Linux 的系统随机数生成 32 字节票据，再编码成 64 个十六进制字符。
bool MakeSessionId(std::string *out)
{
    std::array<unsigned char, 32> bytes;
    size_t offset = 0;
    while (offset < bytes.size())
    {
        ssize_t n = getrandom(bytes.data() + offset, bytes.size() - offset, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        offset += static_cast<size_t>(n);
    }
    const char hex[] = "0123456789abcdef";
    out->clear();
    for (unsigned char byte : bytes)
    {
        out->push_back(hex[byte >> 4]);
        out->push_back(hex[byte & 15]);
    }
    return true;
}

bool CreateSession(int user_id, const std::string &idcount, std::string *session_id)
{
    if (!MakeSessionId(session_id))
        return false;

    MutexModule::LockGuard guard(sessions_mutex);
    const auto now = Clock::now();
    // 顺便清理过期记录。
    for (auto it = sessions.begin(); it != sessions.end();)
    {
        if (it->second.expires_at <= now)
            it = sessions.erase(it);
        else
            ++it;
    }
    Session session{user_id, idcount, now + std::chrono::seconds(kLifetimeSeconds)};
    return sessions.emplace(*session_id, session).second;
}

std::string Trim(const std::string &text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

// 浏览器可能同时携带多个 Cookie，需要按名字找到自己的票据。
std::string GetSessionId(const HTTPRequest &req)
{
    std::istringstream input(req.GetHeader("Cookie"));
    std::string part;
    while (std::getline(input, part, ';'))
    {
        const auto pos = part.find('=');
        if (pos != std::string::npos && Trim(part.substr(0, pos)) == kCookieName)
            return Trim(part.substr(pos + 1));
    }
    return "";
}

bool FindSession(const std::string &session_id, Session *out)
{
    MutexModule::LockGuard guard(sessions_mutex);
    auto it = sessions.find(session_id);
    if (it == sessions.end())
        return false;
    if (it->second.expires_at <= Clock::now())
    {
        sessions.erase(it);
        return false;
    }
    *out = it->second; // 复制结果，解锁后不持有 map 元素的引用。
    return true;
}

void RemoveSession(const std::string &session_id)
{
    MutexModule::LockGuard guard(sessions_mutex);
    sessions.erase(session_id);
}

void ClearCookie(HttpResponse *rsp)
{
    rsp->SetHeader("Set-Cookie", std::string(kCookieName) +
        "=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
}

// 2. 登录成功：保存 Session，通过响应头把票据交给浏览器。
void HandleLogin(const HTTPRequest &req, HttpResponse *rsp)
{
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream input(req._body);
    if (!Json::parseFromStream(reader, input, &root, &errors) || !root.isObject() ||
        !root["idcount"].isString() || !root["idpasswd"].isString())
    {
        ReplyError(rsp, 400, "请提交字符串类型的账号和密码");
        return;
    }

    // 接入你的项目时，将这里替换成 mysql->LogIn(...)，取得真实的 User u。
    if (root["idcount"].asString() != "student" || root["idpasswd"].asString() != "123456")
    {
        ReplyError(rsp, 401, "账号或密码错误");
        return;
    }

    std::string session_id;
    if (!CreateSession(1001, "student", &session_id))
    {
        ReplyError(rsp, 500, "创建登录状态失败");
        return;
    }
    RemoveSession(GetSessionId(req)); // 重新登录后，使这个浏览器原来的票据失效。
    rsp->SetHeader("Set-Cookie", std::string(kCookieName) + "=" + session_id +
        "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" + std::to_string(kLifetimeSeconds));

    Json::Value body(Json::objectValue);
    body["success"] = true;
    body["message"] = "登录成功";
    Reply(rsp, 200, body);
}

// 3. 后续请求：读取 Cookie，找到对应用户。
void HandleMe(const HTTPRequest &req, HttpResponse *rsp)
{
    Session session;
    if (!FindSession(GetSessionId(req), &session))
    {
        ClearCookie(rsp);
        ReplyError(rsp, 401, "尚未登录或登录已过期");
        return;
    }

    Json::Value body(Json::objectValue);
    body["success"] = true;
    body["user"]["id"] = session.user_id;
    body["user"]["idcount"] = session.idcount;
    Reply(rsp, 200, body);
}

// 4. 退出：服务器删除记录，同时让浏览器清除 Cookie。
void HandleLogout(const HTTPRequest &req, HttpResponse *rsp)
{
    RemoveSession(GetSessionId(req));
    ClearCookie(rsp);
    Json::Value body(Json::objectValue);
    body["success"] = true;
    body["message"] = "已退出登录";
    Reply(rsp, 200, body);
}
} // namespace SessionDemo

int main(int argc, char **argv)
{
    int port = 8081;
    try
    {
        if (argc > 2)
            throw std::invalid_argument("参数过多");
        if (argc == 2)
        {
            size_t used;
            const std::string value = argv[1];
            port = std::stoi(value, &used);
            if (used != value.size() || port < 1 || port > 65535)
                throw std::invalid_argument("端口范围错误");
        }
    }
    catch (const std::exception &)
    {
        std::cerr << "用法：./session_demo [1到65535之间的端口，默认8081]\n";
        return 1;
    }

    HttpServer server(port);
    server.SetThreadCount(1);
    server.Post("/api/login", SessionDemo::HandleLogin);
    server.Get("/api/me", SessionDemo::HandleMe);
    server.Post("/api/logout", SessionDemo::HandleLogout);
    std::cout << "Session 演示运行于 http://127.0.0.1:" << port << '\n';
    server.Listen();
    return 0;
}
