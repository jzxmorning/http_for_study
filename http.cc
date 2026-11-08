#include "http.hpp"
#include "Mysql.hpp"
#include <jsoncpp/json/json.h>
#include <sstream>
#include "session.hpp"
MutexModule::Mutex mysql_mutex;

Sessionid session_manger;

// Cookie 属于 HTTP 层，按分号分项、按第一个等号分离名称和值。
std::string GetCookie(const HTTPRequest &req, const std::string &name)
{
    std::istringstream input(req.GetHeader("Cookie"));
    std::string item;
    while (std::getline(input, item, ';'))
    {
        size_t start = item.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        item = item.substr(start, item.find_last_not_of(" \t") - start + 1);
        size_t pos = item.find('=');
        if (pos != std::string::npos && item.substr(0, pos) == name)
            return item.substr(pos + 1);
    }
    return "";
}

void SetCookie(HttpResponse *rsp, const std::string &id)
{
    rsp->SetHeader("Set-Cookie", "sessionid=" + id +
                                     "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" +
                                     std::to_string(SESSION_LIFETIME_SECONDS));
}

void ClearCookie(HttpResponse *rsp)
{
    rsp->SetHeader("Set-Cookie",
                   "sessionid=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
}

void Reply(HttpResponse *rsp, int status, const Json::Value &body)
{
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    rsp->_statu = status;
    rsp->SetHeader("Cache-Control", "no-store");
    rsp->SetContent(Json::writeString(writer, body), "application/json;charset=utf-8");
}
void ReplyError(HttpResponse *rsp, int status, const std::string message)
{
    Json::Value body(Json::objectValue);
    body["success"] = false;
    body["message"] = message;
    Reply(rsp, status, body);
}

session_ptr RequireLogin(const HTTPRequest &req, HttpResponse *rsp)
{
    auto session = session_manger.FindSession(GetCookie(req, "sessionid"));
    if (!session)
    {
        ClearCookie(rsp);
        ReplyError(rsp, 401, "登录已失效，请重新登录");
    }
    return session;
}

void HandleMe(const HTTPRequest &req, HttpResponse *rsp)
{
    auto session = RequireLogin(req, rsp);
    if (!session)
        return;
    Json::Value body;
    body["success"] = true;
    body["user"]["id"] = session->GetUserId();
    body["user"]["idcount"] = session->GetUsername();
    body["user"]["name"] = session->GetName();
    body["user"]["identity"] = session->GetIdentity();
    Reply(rsp, 200, body);
}

void HandleLogout(const HTTPRequest &req, HttpResponse *rsp)
{
    session_manger.RemoveSession(GetCookie(req, "sessionid"));
    ClearCookie(rsp);
    Json::Value body;
    body["success"] = true;
    body["message"] = "已退出登录";
    Reply(rsp, 200, body);
}
void HandleLogIn(const HTTPRequest &req, HttpResponse *rsp, MySQL *mysql)
{
    // 1. 解析登录请求中的 JSON
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string error;
    std::istringstream input(req._body);

    if (!Json::parseFromStream(builder, input, &root, &error) ||
        !root.isObject() ||
        !root["idcount"].isString() ||
        !root["idpasswd"].isString())
    {
        ReplyError(rsp, 400, "JSON格式错误或缺少账号密码");
        return;
    }

    std::string idcount = root["idcount"].asString();
    std::string idpasswd = root["idpasswd"].asString();

    if (idcount.empty() || idpasswd.empty())
    {
        ReplyError(rsp, 400, "账号和密码不能为空");
        return;
    }

    // 2. 查询数据库，验证账号密码
    User u;
    bool ret;
    {
        MutexModule::LockGuard guard(mysql_mutex);
        ret = mysql->LogIn(idcount, idpasswd, &u);
    }

    if (!ret)
    {
        ReplyError(rsp, 401, "账号或密码错误");
        return;
    }

    // 3. 验证成功，创建新的 Session
    std::string new_sessionid;
    try
    {
        auto session = std::make_shared<Session>(
            u.idcount, u._identity, STATUS::LOGED, u.id, u.name);
        // 获取该用户的做题记录
        std::vector<QuestionProgress> q;
        bool ret2;
        {
            MutexModule::LockGuard guard(mysql_mutex);
            ret2 = mysql->SearchUserProgress(u.id, &q);
        }
        if (ret2 == false)
        {
            ReplyError(rsp, 500, "获取用户做题记录失败");
            return;
        }
        else
        {
            // 将获取的做题记录写进用户的session中
            for (auto &it : q)
            {
                session->progress.insert({it.question_id, it});
            }
        }
        new_sessionid = session_manger.AddSession(session);
    }
    catch (const std::exception &)
    {
        ReplyError(rsp, 500, "创建登录状态失败，请重试");
        return;
    }

    // 4. 删除当前浏览器原来使用的 Session
    std::string old_sessionid = GetCookie(req, "sessionid");
    if (!old_sessionid.empty())
    {
        session_manger.RemoveSession(old_sessionid);
    }

    // 5. 将新编号写入 Cookie
    SetCookie(rsp, new_sessionid);

    // 6.构建响应
    Json::Value body;
    body["success"] = true;
    body["message"] = "登录成功";
    Reply(rsp, 200, body);
}
void HandelRegister(const HTTPRequest &req, HttpResponse *rsp, MySQL *mysql)
{
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string error;
    std::istringstream input(req._body);
    if (!Json::parseFromStream(builder, input, &root, &error) || !root.isObject() || !root["idcount"].isString() || !root["idpasswd"].isString() || !root["name"].isString())
    {
        ReplyError(rsp, 400, "JSON格式错误或缺少昵称、账号、密码");
        return;
    }
    std::string name = root["name"].asString();
    std::string idcount = root["idcount"].asString();
    std::string idpasswd = root["idpasswd"].asString();
    // 公开注册仅创建学生账号，教师身份不能由客户端自行指定。
    if (root.isMember("identity") &&
        (!root["identity"].isString() || root["identity"].asString() != "student"))
    {
        ReplyError(rsp, 403, "公开注册仅支持学生账号");
        return;
    }
    const std::string identity = "student";
    if (name.find_first_not_of(" \t\r\n") == std::string::npos ||
        idcount.find_first_not_of(" \t\r\n") == std::string::npos || idpasswd.empty())
    {
        ReplyError(rsp, 400, "称呼、密码、账号、身份不能为空");
        return;
    }
    bool ret;
    {
        MutexModule::LockGuard guard(mysql_mutex);
        ret = mysql->Register(idcount, idpasswd, identity, name);
    }
    if (ret == false)
    {
        ReplyError(rsp, 400, "注册失败,请重试");
        return;
    }
    Json::Value v;
    v["success"] = true;
    v["message"] = "注册成功";
    Reply(rsp, 200, v);
    return;
}
void HandleAllquestions(const HTTPRequest &req, HttpResponse *rsp, MySQL *mysql)
{
    auto it = RequireLogin(req, rsp);
    if (it == nullptr)
    {
        return;
    }
    else
    {
        it->SetStatus(STATUS::PRACTICE);
        std::vector<Questions> out;
        bool ret;
        {
            MutexModule::LockGuard guard(mysql_mutex);
            ret = mysql->GetQuestions(&out);
        }
        if (ret == false)
        {
            ReplyError(rsp, 500, "获取题目列表失败");
            return;
        }
        // 通过session获取题目的做题记录
        Json::Value va;
        va["success"] = true;
        va["questions"] = Json::Value(Json::arrayValue);
        for (const auto &v : out)
        {
            Json::Value item(Json::objectValue);
            item["id"] = v.id;
            item["title"] = v.title;
            item["grade"] = v.grade;
            auto pos = it->progress.find(v.id);
            if (pos == it->progress.end())
            {
                item["attempt"] = 0;
                item["correct_count"] = 0;
                item["last_answer"] = "";
                item["status"] = "未作答";
            }
            else
            {
                item["attempt"] = it->progress[v.id].attempt_count;
                item["correct_count"] = it->progress[v.id].correct_count;
                item["last_answer"] = it->progress[v.id].last_answer;
                item["last_correct"] = it->progress[v.id].last_correct;
                item["last_answer_at"] = it->progress[v.id].last_answer_at;
            }
            va["questions"].append(item);
        }
        Reply(rsp, 200, va);
        return;
    }
}
void HandleOneQuestion(const HTTPRequest &req, HttpResponse *rsp, MySQL *mysql)
{
    if (!RequireLogin(req, rsp))
        return;
    int id;
    try
    {
        id = std::stoi(req._matches[1].str());
    }
    catch (const std::exception &)
    {
        ReplyError(rsp, 400, "题目编号无效");
        return;
    }
    Questions q;
    bool ret;
    {
        MutexModule::LockGuard guard(mysql_mutex);
        ret = mysql->SerchQuestion(id, &q);
    }
    if (ret == false)
    {
        ReplyError(rsp, 500, "获取题目失败");
        return;
    }
    Json::Value v;
    v["success"] = true;
    v["question"] = Json::Value(Json::objectValue);
    Json::Value v1;
    v1["id"] = q.id;
    v1["title"] = q.title;
    v1["choice"] = q.choice;
    v1["grade"] = q.grade;
    v["question"] = v1;
    Reply(rsp, 200, v);
    return;
}
void Judge(const HTTPRequest &req, HttpResponse *rsp, MySQL *mysql)
{
    auto session = RequireLogin(req, rsp);
    if (!session)
        return;
    session->SetStatus(STATUS::PRACTICE);
    int id;
    try
    {
        id = std::stoi(req._matches[1].str());
    }
    catch (const std::exception &)
    {
        ReplyError(rsp, 400, "题目编号无效");
        return;
    }
    Questions q;
    bool ret;
    {
        MutexModule::LockGuard guard(mysql_mutex);
        ret = mysql->SerchQuestion(id, &q);
    }
    if (ret == false)
    {
        ReplyError(rsp, 500, "获取题目失败");
        return;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string error;
    std::istringstream input(req._body);
    if (!Json::parseFromStream(builder, input, &root, &error) || !root.isObject() || !root["answer"].isString())
    {
        ReplyError(rsp, 400, "获取用户的答案失败");
        return;
    }
    std::string answer = root["answer"].asString();
    if (Questions::NormalizeAnswer(answer).empty())
    {
        ReplyError(rsp, 400, "提供的答案不能为空");
        return;
    }
    Json::Value v;
    if (q.Match(answer))
    {
        v["success"] = true;
        v["correct"] = true;
        v["message"] = "回答正确";
        session->RecordAnswer(id, answer, true);
    }
    else
    {
        v["success"] = true;
        v["correct"] = false;
        v["message"] = "回答错误";
        session->RecordAnswer(id, answer, false);
    }
    Reply(rsp, 200, v);
    return;
}
void HandleShowUser(const HTTPRequest &req, HttpResponse *rsp)
{
    // 通过cookie来得到用户
    auto session = RequireLogin(req, rsp);
    if (session == nullptr)
    {
        return;
    }
    else
    {
        Json::Value root;
        root["success"] = true;

        Json::Value user(Json::objectValue);
        user["idcount"] = session->GetUsername();  // 登录账号
        user["name"] = session->GetName();         // 昵称
        user["identity"] = session->GetIdentity(); // 身份
        root["user"] = user;
        Reply(rsp, 200, root);
    }
}
void SaveProgress(const HTTPRequest &req, HttpResponse *rsp, MySQL *mysql)
{
    auto session = RequireLogin(req, rsp);
    if (session == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> progress_guard(session->progress_mutex);
    std::vector<QuestionProgress> pending;
    for (auto &q : session->progress)
    {
        if (q.second.dirty == true)
        {
            pending.push_back(q.second);
        }
    }
    if(pending.empty()==false)
    {
        bool save;
        {
            MutexModule::LockGuard guard(mysql_mutex);
            save = mysql->SaveUserProgress(session->user_id,pending);
        }
       if (!save)
        {
            // 保留 dirty，下次仍然可以重试。
            ReplyError(rsp, 500, "保存作答记录失败，请重试");
            return;
        }

        //全部设置为已经保存
        for (const auto &record : pending)
        {
            session->progress.at(record.question_id).dirty = false;
        }
    }
    Json::Value body;
    body["success"] = true;
    body["saved_count"] = Json::UInt64(pending.size());
    body["message"] = pending.empty()
        ? "没有需要保存的修改"
        : "作答记录保存成功";

    Reply(rsp, 200, body);
}
int main()
{
    MySQL my;
    if (!my.Connect_mysql())
        return 1;
    HttpServer server(8080);
    server.SetThreadCount(1);
    server.SetBaseDir("./wwwroot");
    server.Get("/api/me", HandleShowUser);
    server.Get("/api/showuser", HandleShowUser);
    server.Post("/api/progress/save", [&my](const HTTPRequest &req, HttpResponse *rsp)
                { SaveProgress(req, rsp, &my); });
    server.Post("/api/logout", HandleLogout);
    server.Post("/api/login",
                [&my](const HTTPRequest &req, HttpResponse *rsp)
                {
                    HandleLogIn(req, rsp, &my);
                });
    server.Post("/api/register", [&my](const HTTPRequest &req, HttpResponse *rsp)
                { HandelRegister(req, rsp, &my); });
    server.Get("/api/allquestions", [&my](const HTTPRequest &req, HttpResponse *rsp)
               { HandleAllquestions(req, rsp, &my); });
    server.Get("/api/question/([0-9]+)", [&my](const HTTPRequest &req, HttpResponse *rsp)
               { HandleOneQuestion(req, rsp, &my); });
    server.Post("/api/question/([0-9]+)/judge", [&my](const HTTPRequest &req, HttpResponse *rsp)
                { Judge(req, rsp, &my); });
    server.Listen();
    return 0;
}
