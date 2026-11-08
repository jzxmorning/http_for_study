// 使用内存中的数据库替身验证实际处理函数，不连接或修改真实 MySQL。
#include "../Mysql.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <regex>
#include <unordered_map>

class TestMySQL
{
public:
    int queries = 0;
    std::string registered_name;
    std::string registered_identity;
    bool fail_save = false;
    int save_calls = 0;
    std::unordered_map<int, std::vector<QuestionProgress>> stored_progress;
    bool Connect_mysql() { return true; }
    bool SearchUserProgress(int userid, std::vector<QuestionProgress> *out)
    {
        ++queries;
        *out = stored_progress[userid];
        return true;
    }
    bool SaveUserProgress(int userid, const std::vector<QuestionProgress> &records)
    {
        ++queries;
        ++save_calls;
        if (fail_save) return false;
        auto &stored = stored_progress[userid];
        for (auto record : records)
        {
            record.dirty = false;
            record.version = 0;
            auto it = std::find_if(stored.begin(), stored.end(), [&](const QuestionProgress &old) {
                return old.question_id == record.question_id;
            });
            if (it == stored.end()) stored.push_back(record);
            else *it = record;
        }
        return true;
    }
    bool LogIn(const std::string &account, const std::string &password, User *out)
    {
        ++queries;
        if (account != "student" || password != "test-password") return false;
        out->id = 42;
        out->idcount = account;
        out->name = "测试同学";
        out->_identity = "student";
        return true;
    }
    bool Register(const std::string &, const std::string &,
                  const std::string &identity, const std::string &name)
    {
        ++queries;
        registered_name = name;
        registered_identity = identity;
        return true;
    }
    bool SerchQuestion(int id, Questions *out)
    {
        ++queries;
        if (id != 1) return false;
        out->id = 1;
        out->title = "C++ 中表示相等比较的运算符是？";
        out->choice = "A. ==\nB. =\nC. !=\nD. +=";
        out->answer = "A";
        out->grade = "5";
        return true;
    }
    bool GetQuestions(std::vector<Questions> *out)
    {
        Questions q;
        SerchQuestion(1, &q);
        out->push_back(q);
        return true;
    }
};

#define MySQL TestMySQL
#define main ProductionMain
#include "../http.cc"
#undef main
#undef MySQL

Json::Value ReadBody(const HttpResponse &rsp)
{
    Json::Value body;
    Json::CharReaderBuilder builder;
    std::string error;
    std::istringstream input(rsp._body);
    assert(Json::parseFromStream(builder, input, &body, &error));
    return body;
}

std::string SessionCookie(HttpResponse &rsp)
{
    auto cookie = rsp.GetHeader("Set-Cookie");
    assert(cookie.find("HttpOnly") != std::string::npos);
    assert(cookie.find("SameSite=Lax") != std::string::npos);
    assert(cookie.find("Max-Age=1800") != std::string::npos);
    return cookie.substr(0, cookie.find(';'));
}

int main(int argc, char **argv)
{
    TestMySQL db;
    if (argc == 3 && std::string(argv[1]) == "--serve")
    {
        HttpServer server(std::stoi(argv[2]));
        server.SetThreadCount(2);
        server.SetBaseDir("./wwwroot");
        server.Get("/api/me", HandleShowUser);
        server.Get("/api/showuser", HandleShowUser);
        server.Post("/api/progress/save", [&](const HTTPRequest &r, HttpResponse *s) { SaveProgress(r, s, &db); });
        server.Post("/api/logout", HandleLogout);
        server.Post("/api/login", [&](const HTTPRequest &r, HttpResponse *s) { HandleLogIn(r, s, &db); });
        server.Post("/api/register", [&](const HTTPRequest &r, HttpResponse *s) { HandelRegister(r, s, &db); });
        server.Get("/api/allquestions", [&](const HTTPRequest &r, HttpResponse *s) { HandleAllquestions(r, s, &db); });
        server.Get("/api/question/([0-9]+)", [&](const HTTPRequest &r, HttpResponse *s) { HandleOneQuestion(r, s, &db); });
        server.Post("/api/question/([0-9]+)/judge", [&](const HTTPRequest &r, HttpResponse *s) { Judge(r, s, &db); });
        server.Listen();
        return 0;
    }

    assert(Util::UrlDecode("a%20b%2Fc", false) == "a b/c");
    assert(Util::UrlDecode("%GG%?0%", false) == "%GG%?0%");
    HTTPRequest req;
    HttpResponse rsp;
    HandleAllquestions(req, &rsp, &db);
    assert(rsp._statu == 401 && db.queries == 0 && !rsp._redirect_flag);
    assert(!ReadBody(rsp)["success"].asBool());
    rsp.Reset(); HandleOneQuestion(req, &rsp, &db);
    assert(rsp._statu == 401 && db.queries == 0);
    rsp.Reset(); Judge(req, &rsp, &db);
    assert(rsp._statu == 401 && db.queries == 0);
    rsp.Reset(); HandleShowUser(req, &rsp);
    assert(rsp._statu == 401 && db.queries == 0);
    rsp.Reset(); SaveProgress(req, &rsp, &db);
    assert(rsp._statu == 401 && db.queries == 0);

    req._body = "{";
    rsp.Reset(); HandleLogIn(req, &rsp, &db);
    assert(rsp._statu == 400 && db.queries == 0);
    req._body = R"({"idcount":"student","idpasswd":"wrong"})";
    rsp.Reset(); HandleLogIn(req, &rsp, &db);
    assert(rsp._statu == 401);

    req.SetHeader("Cookie", "theme=dark; sessionid=old-invalid-cookie");
    req._body = R"({"idcount":"student","idpasswd":"test-password"})";
    rsp.Reset(); HandleLogIn(req, &rsp, &db);
    assert(rsp._statu == 200 && ReadBody(rsp)["success"].asBool());
    auto old_cookie = SessionCookie(rsp);
    req._headers["Cookie"] = old_cookie;
    rsp.Reset(); HandleMe(req, &rsp);
    auto body = ReadBody(rsp);
    assert(rsp._statu == 200 && body["user"]["id"].asInt() == 42);
    assert(body["user"]["name"].asString() == "测试同学");
    assert(!body["user"].isMember("passwd") && !body["user"].isMember("idpasswd"));
    rsp.Reset(); HandleShowUser(req, &rsp);
    body = ReadBody(rsp);
    assert(rsp._statu == 200 && body["success"].asBool());
    assert(body["user"]["idcount"].asString() == "student");
    assert(body["user"]["name"].asString() == "测试同学");
    assert(body["user"]["identity"].asString() == "student");
    rsp.Reset(); HandleLogIn(req, &rsp, &db);
    auto cookie = SessionCookie(rsp);
    assert(cookie != old_cookie);
    rsp.Reset(); HandleMe(req, &rsp);
    assert(rsp._statu == 401); // 重新登录已经撤销旧编号。

    req._headers["Cookie"] = cookie;
    rsp.Reset(); HandleAllquestions(req, &rsp, &db);
    assert(rsp._statu == 200 && ReadBody(rsp)["questions"].size() == 1);
    assert(!rsp.HasHeader("Set-Cookie")); // 浏览题目不延长固定有效期。
    req._path = "/api/question/1/judge";
    std::regex_match(req._path, req._matches, std::regex("/api/question/([0-9]+)/judge"));
    req._body = R"({"answer":" a "})";
    rsp.Reset(); Judge(req, &rsp, &db);
    assert(ReadBody(rsp)["correct"].asBool());
    req._body = R"({"answer":"B"})";
    rsp.Reset(); Judge(req, &rsp, &db);
    assert(rsp._statu == 200 && !ReadBody(rsp)["correct"].asBool());
    req._body = R"({"answer":"  "})";
    rsp.Reset(); Judge(req, &rsp, &db);
    assert(rsp._statu == 400);
    rsp.Reset(); HandleAllquestions(req, &rsp, &db);
    body = ReadBody(rsp);
    assert(body["questions"][0]["attempt"].asInt() == 2);
    assert(body["questions"][0]["correct_count"].asInt() == 1);
    assert(body["questions"][0]["last_answer"].asString() == "B");
    assert(!body["questions"][0]["last_correct"].asBool());
    assert(body["questions"][0]["last_answer_at"].asInt64() > 0);

    auto active = session_manger.FindSession(GetCookie(req, "sessionid"));
    assert(active && active->progress.at(1).dirty);
    db.fail_save = true;
    rsp.Reset(); SaveProgress(req, &rsp, &db);
    assert(rsp._statu == 500 && active->progress.at(1).dirty);
    db.fail_save = false;
    rsp.Reset(); SaveProgress(req, &rsp, &db);
    assert(rsp._statu == 200 && ReadBody(rsp)["saved_count"].asUInt64() == 1);
    assert(!active->progress.at(1).dirty);
    assert(db.stored_progress.at(42)[0].attempt_count == 2);
    int saves = db.save_calls;
    rsp.Reset(); SaveProgress(req, &rsp, &db);
    assert(rsp._statu == 200 && ReadBody(rsp)["saved_count"].asUInt64() == 0);
    assert(db.save_calls == saves);

    rsp.Reset(); HandleLogout(req, &rsp);
    assert(rsp._statu == 200 && rsp.GetHeader("Set-Cookie").find("Max-Age=0") != std::string::npos);
    rsp.Reset(); HandleMe(req, &rsp);
    assert(rsp._statu == 401); // 重放退出前的 Cookie 也会失败。
    rsp.Reset(); HandleLogout(req, &rsp);
    assert(rsp._statu == 200);

    req._body = R"({"name":"昵称","idcount":"new","idpasswd":"pass","identity":"teacher"})";
    int before = db.queries;
    rsp.Reset(); HandelRegister(req, &rsp, &db);
    assert(rsp._statu == 403 && db.queries == before);
    req._body = R"({"name":"昵称","idcount":"new","idpasswd":"pass"})";
    rsp.Reset(); HandelRegister(req, &rsp, &db);
    assert(rsp._statu == 200 && ReadBody(rsp)["success"].asBool());
    assert(db.registered_identity == "student" && db.registered_name == "昵称");
    std::cout << "HTTP handlers: login, auth, judging, save/retry, clean flags and logout passed\n";
}
