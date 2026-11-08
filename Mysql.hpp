#pragma once
#include <cstdlib>
#include <iostream>
#include <vector>
#include <mysql/mysql.h>
#include "Log.hpp"
#include "Mutex.hpp"
#include "userandquestions.hpp"
#include "mysql_password.local.hpp"
using namespace LogMoudle;
const std::string host = "127.0.0.1";
const std::string user = "root";
const std::string passwd = jiao_mysql_config::password;
const std::string db = "oj_jiao";
const std::string port = "3306";
const std::string user_table_name = "user_table";
const std::string questions_table_name = "questions_table";
class MySQL
{
private:
    MYSQL *_mysql;
    bool _connected = false;

private:
    // 修改成绩
    bool UpdataGrade(int id, std::string newgrade)
    {
        if (!_connected)
        {
            return false;
        }
        std::string sql = "update " + std::string(user_table_name) + " set grade = '" + Escape(newgrade) + "' where id = " + std::to_string(id);
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "修改成绩失败: " << mysql_error(_mysql);
            LOG(LogLevel::ERROR) << "SQL语句: " << sql;
            return false;
        }
        if (mysql_affected_rows(_mysql) == 0)
        {
            LOG(LogLevel::WARNING) << "ID=" << id << " 不存在，未修改成绩";
            return false;
        }
        return true;
    }
    // 修改密码
    bool UpdataPasswd(int id, std::string newpasswd)
    {
        if (!_connected)
        {
            return false;
        }
        if (newpasswd.empty())
        {
            LOG(LogLevel::ERROR) << "修改失败：新密码不能为空";
            return false;
        }
        std::string sql = "update " + std::string(user_table_name) + " set idpasswd = '" + Escape(newpasswd) + "' where id = " + std::to_string(id);
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "修改密码失败: " << mysql_error(_mysql);
            LOG(LogLevel::ERROR) << "SQL语句: " << sql;
            return false;
        }
        if (mysql_affected_rows(_mysql) == 0)
        {
            LOG(LogLevel::WARNING) << "ID=" << id << " 不存在，未修改密码";
            return false;
        }
        return true;
    }
    // 删除用户
    bool Delete(int id)
    {
        if (!_connected)
        {
            return false;
        }
        std::string sql = "delete from " + std::string(user_table_name) + " where id = " + std::to_string(id);
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "删除失败: " << mysql_error(_mysql);
            LOG(LogLevel::ERROR) << "SQL语句: " << sql;
            return false;
        }
        if (mysql_affected_rows(_mysql) == 0)
        {
            LOG(LogLevel::WARNING) << "ID=" << id << " 不存在，未删除任何记录";
            return false;
        }
        return true;
    }

public:
    MySQL()
    {
        _mysql = mysql_init(nullptr);
        if (_mysql == nullptr)
        {
            LOG(LogLevel::ERROR) << "mysql_init 失败(内存不足)";
        }
    }
    MySQL(const MySQL &) = delete;
    MySQL &operator=(const MySQL &) = delete;
    std::string Escape(const std::string &s)
    {
        if (!_connected || s.empty())
        {
            return "";
        }
        std::string out(s.size() * 2 + 1, '\0');
        unsigned long n = mysql_real_escape_string(_mysql, &out[0], s.c_str(),
                                                   (unsigned long)s.size());
        out.resize(n);
        return out;
    }
    bool Connect_mysql(std::string host = "127.0.0.1", std::string user = "root", std::string passwd = ::passwd, std::string db = "oj_jiao", int port = 3306)
    {
        if (_mysql == nullptr)
            return false;
        if (mysql_real_connect(_mysql, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, CLIENT_FOUND_ROWS) == nullptr)
        {
            LOG(LogLevel::ERROR) << "连接数据库失败: " << mysql_error(_mysql);
            return false;
        }
        if (mysql_set_character_set(_mysql, "utf8mb4") != 0)
        {
            LOG(LogLevel::ERROR) << "设置字符集失败: " << mysql_error(_mysql);
            return false;
        }
        _connected = true;
        LOG(LogLevel::INFO) << "成功连接数据库";
        return true;
    }
    bool LogIn(const std::string idcount, const std::string idpasswd, User *out)
    {
        if (!_connected || out == nullptr)
            return false;
        std::string sql = "select id, idcount, identity, idpasswd, grade, name from " + std::string(user_table_name) + " where idcount = '" + Escape(idcount) + "' and idpasswd = '" + Escape(idpasswd) + "'";
        return ExcuteSQLInUser(sql, out);
    }
    bool Register(const std::string idcount, const std::string idpasswd,
                  std::string identity, const std::string &name,
                  const std::string grade = "0")
    {
        if (!_connected)
            return false;

        if (idcount.empty() || idpasswd.empty() || identity.empty() || name.empty())
        {
            LOG(LogLevel::ERROR) << "注册失败: 账号/密码/身份/昵称均不能为空"; // 顺手修掉双斜杠和残留的"id"
            return false;
        }

        // id 列省略 → 由表结构的 AUTO_INCREMENT 自动生成
        // 值清单 5 列 5 值，顺序与列一一对应：
        //   idcount, identity, idpasswd, grade, name
        std::string sql =
            "insert into " + std::string(user_table_name) +
            "(idcount, identity, idpasswd, grade, name) "
            "select '" +
            Escape(idcount) + "', "
                              "'" +
            Escape(identity) + "', "
                               "'" +
            Escape(idpasswd) + "', "
                               "'" +
            Escape(grade) + "', "
                            "'" +
            Escape(name) + "' from dual "
                           "where '" +
            Escape(idcount) + "' not in "
                              "(select idcount from " +
            std::string(user_table_name) + ")";

        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "注册失败: " << mysql_error(_mysql);
            return false;
        }
        return mysql_affected_rows(_mysql) > 0;
    }
    bool ExcuteSQLInUser(const std::string sql, User *out)
    {
        if (!_connected || out == nullptr)
        {
            return false;
        }
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "执行sql语句失败: " << mysql_error(_mysql)
                                 << " (错误码: " << mysql_errno(_mysql) << ")";
            return false;
        }
        MYSQL_RES *res = mysql_store_result(_mysql);
        if (res == nullptr)
        {
            LOG(LogLevel::ERROR) << "获取查询结果失败";
            return false;
        }
        MYSQL_ROW row_data = mysql_fetch_row(res);
        if (row_data == nullptr)
        {
            mysql_free_result(res);
            return false;
        }
        User u;
        u.id = row_data[0] ? std::atoi(row_data[0]) : 0;
        u.idcount = row_data[1] ? row_data[1] : "";
        u._identity = row_data[2] ? row_data[2] : "";
        u.passwd = row_data[3] ? row_data[3] : "";
        u.grade = row_data[4] ? std::atoi(row_data[4]) : 0;
        if (mysql_num_fields(res) > 5)
            u.name = row_data[5] ? row_data[5] : "";
        *out = u;
        mysql_free_result(res);
        return true;
    }

    // 给老师可以改学生成绩、密码以及删除的接口
    // 1->修改成绩 2->修改密码  3->删除用户
    bool UpdataUser(int id, int op, std::string value = "")
    {
        switch (op)
        {
        case 1:
            return !value.empty() && UpdataGrade(id, value);
        case 2:
            return UpdataPasswd(id, value);
        case 3:
            return Delete(id);
        default:
            return false;
        }
    }

    // 添加用户
    bool AddUser(const std::string id, const std::string idcount, const std::string idpasswd, std::string identity, const std::string &name, const std::string grade = "0")
    {
        return Register(idcount, idpasswd, identity, name, grade);
    }

    bool ShowUsers(std::vector<User> *out)
    {
        if (!_connected || out == nullptr)
        {
            return false;
        }
        std::string sql = "select id, idcount, identity, idpasswd, grade from " + user_table_name;
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::INFO) << sql;
            LOG(LogLevel::ERROR) << "执行sql语句失败: " << mysql_error(_mysql) << " (错误码: " << mysql_errno(_mysql) << ")";
            LOG(LogLevel::ERROR) << "SQL语句: " << sql;
            return false;
        }
        MYSQL_RES *res = mysql_store_result(_mysql);
        if (res == nullptr)
        {
            LOG(LogLevel::ERROR) << "获取查询结果失败";
            return false;
        }
        int rows = mysql_num_rows(res);
        out->clear();
        User u;
        for (int i = 0; i < rows; i++)
        {
            MYSQL_ROW row_data = mysql_fetch_row(res);
            if (row_data == nullptr)
                break;
            u.id = row_data[0] ? std::atoi(row_data[0]) : 0;
            u.idcount = row_data[1] ? row_data[1] : "";
            u._identity = row_data[2] ? row_data[2] : "";
            u.passwd = row_data[3] ? row_data[3] : "";
            u.grade = row_data[4] ? std::atoi(row_data[4]) : 0;
            out->push_back(u);
        }
        mysql_free_result(res);
        return true;
    }

public:
    // 获取实际存在的题目，避免把不连续的主键当作 1..COUNT(*)。
    bool GetQuestions(std::vector<Questions> *out)
    {
        if (!_connected || out == nullptr)
            return false;
        const std::string sql = "select id, title, choice, answer, grade from " + questions_table_name + " order by id";
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "查询题库失败: " << mysql_error(_mysql);
            return false;
        }
        MYSQL_RES *res = mysql_store_result(_mysql);
        if (res == nullptr)
            return false;
        out->clear();
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            Questions q;
            q.id = row[0] ? std::atoi(row[0]) : 0;
            q.title = row[1] ? row[1] : "";
            q.choice = row[2] ? row[2] : "";
            q.answer = row[3] ? row[3] : "";
            q.grade = row[4] ? row[4] : "0";
            out->push_back(q);
        }
        mysql_free_result(res);
        return true;
    }
    bool SerchQuestion(int id, Questions *out)
    {
        std::string sql = "select id, title, choice, answer, grade from " + std::string(questions_table_name) + " where id = " + std::to_string(id);
        return ExcuteSQLInQuestions(sql, out);
    }
    bool ExcuteSQLInQuestions(const std::string sql, Questions *out)
    {
        if (!_connected || out == nullptr)
        {
            return false;
        }
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::INFO) << sql;
            LOG(LogLevel::ERROR) << "执行sql语句失败: " << mysql_error(_mysql) << " (错误码: " << mysql_errno(_mysql) << ")";
            LOG(LogLevel::ERROR) << "SQL语句: " << sql;
            return false;
        }
        MYSQL_RES *res = mysql_store_result(_mysql);
        if (res == nullptr)
        {
            LOG(LogLevel::ERROR) << "获取查询结果失败";
            return false;
        }
        Questions q;
        MYSQL_ROW row_data = mysql_fetch_row(res);
        if (row_data == nullptr)
        {
            mysql_free_result(res);
            return false;
        }
        q.id = row_data[0] ? std::atoi(row_data[0]) : 0;
        q.title = row_data[1] ? row_data[1] : "";
        q.choice = row_data[2] ? row_data[2] : "";
        q.answer = row_data[3] ? row_data[3] : "";
        q.grade = row_data[4] ? row_data[4] : "0";
        *out = q;
        mysql_free_result(res);
        return true;
    }
    // 获取有效题目的个数
    int Question_count()
    {
        if (!_connected)
        {
            return -1;
        }
        std::string sql = "SELECT COUNT(*) FROM " + questions_table_name;
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::INFO) << sql;
            LOG(LogLevel::ERROR) << "执行sql语句失败: " << mysql_error(_mysql) << " (错误码: " << mysql_errno(_mysql) << ")";
            LOG(LogLevel::ERROR) << "SQL语句: " << sql;
            return -1;
        }
        MYSQL_RES *res = mysql_store_result(_mysql);
        if (res == nullptr)
        {
            LOG(LogLevel::ERROR) << "获取查询结果失败: " << mysql_error(_mysql);
            return -1;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == nullptr)
        {
            mysql_free_result(res);
            LOG(LogLevel::ERROR) << "查询结果为空";
            return -1;
        }
        int count = std::atoi(row[0]);
        mysql_free_result(res);
        return count;
    }

    // 添加试题：题干/选项/答案/分值 全部转义后入库，防 SQL 注入
    bool AddQuestion(const std::string title, const std::string choice,
                     const std::string answer, const std::string grade)
    {
        if (!_connected)
            return false;
        const std::string normalized_answer = Questions::NormalizeAnswer(answer);
        if (title.empty() || choice.empty() || normalized_answer.empty())
        {
            LOG(LogLevel::ERROR) << "添加试题失败：题干/选项/答案不能为空";
            return false;
        }
        std::string sql =
            "insert into " + questions_table_name + "(title, choice, answer, grade) values ('" + Escape(title) + "', '" + Escape(choice) + "', '" + Escape(normalized_answer) + "', '" + Escape(grade) + "')";
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "添加试题失败: " << mysql_error(_mysql);
            return false;
        }
        return mysql_affected_rows(_mysql) > 0;
    }
    bool DeleteQuestion(int id)
    {
        if (!_connected)
            return false;
        std::string sql = "delete from " + questions_table_name + " where id = " + std::to_string(id);
        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR) << "删除失败: " << mysql_error(_mysql);
            return false;
        }
        if (mysql_affected_rows(_mysql) == 0)
        {
            LOG(LogLevel::WARNING) << "ID=" << id << " 不存在，未删除任何记录";
            return false;
        }
        return true;
    }

    ~MySQL()
    {
        if (_mysql != nullptr)
        {
            mysql_close(_mysql);
            _mysql = nullptr;
        }
    }

public:
    bool SearchUserProgress(int userid, std::vector<QuestionProgress> *out)
    {
        if (_mysql == nullptr || !_connected || out == nullptr)
        {
            return false;
        }

        std::string sql =
            "SELECT question_id, attempt_count, correct_count, "
            "last_answer, last_correct "
            "FROM user_question_progress "
            "WHERE user_id = " +
            std::to_string(userid);

        if (mysql_query(_mysql, sql.c_str()) != 0)
        {
            LOG(LogLevel::ERROR)
                << "查询用户作答状态失败: " << mysql_error(_mysql);
            return false;
        }

        MYSQL_RES *res = mysql_store_result(_mysql);
        if (res == nullptr)
        {
            LOG(LogLevel::ERROR)
                << "获取查询结果失败: " << mysql_error(_mysql);
            return false;
        }

        std::vector<QuestionProgress> result;
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            QuestionProgress q{};

            q.question_id =
                row[0] ? std::atoi(row[0]) : 0;

            q.attempt_count =
                row[1] ? std::atoi(row[1]) : 0;

            q.correct_count =
                row[2] ? std::atoi(row[2]) : 0;

            q.last_answer = row[3] ? row[3] : "";

            q.last_correct =
                row[4] && std::atoi(row[4]) != 0;

            result.push_back(q);
        }

        mysql_free_result(res);

        out->swap(result);
        return true;
    }

   bool SaveUserProgress(
    int userid,
    const std::vector<QuestionProgress> &vec)
{
    if (_mysql == nullptr || !_connected || userid <= 0)
    {
        return false;
    }

    if (vec.empty())
    {
        return true;
    }

    // 多道题作为一次保存：全部成功才提交。
    if (mysql_query(_mysql, "START TRANSACTION") != 0)
    {
        LOG(LogLevel::ERROR)
            << "开启保存事务失败: " << mysql_error(_mysql);
        return false;
    }

    try
    {
        for (const auto &q : vec)
        {
            std::string attempts = std::to_string(q.attempt_count);
            std::string corrects = std::to_string(q.correct_count);

            // 加引号，并转义用户提交的答案。
            std::string answer = "'" + Escape(q.last_answer) + "'";

            std::string correct = q.last_correct ? "1" : "0";

            // 将时间戳秒数转换成数据库日期时间。
            std::string answered_at =
                "FROM_UNIXTIME(" +
                std::to_string(q.last_answer_at) + ")";

            std::string sql =
                "INSERT INTO user_question_progress "
                "(user_id, question_id, attempt_count, correct_count, "
                "last_answer, last_correct, last_answer_at) "
                "VALUES (" +
                std::to_string(userid) + ", " +
                std::to_string(q.question_id) + ", " +
                attempts + ", " +
                corrects + ", " +
                answer + ", " +
                correct + ", " +
                answered_at + ") "
                "ON DUPLICATE KEY UPDATE "
                "attempt_count = " + attempts + ", "
                "correct_count = " + corrects + ", "
                "last_answer = " + answer + ", "
                "last_correct = " + correct + ", "
                "last_answer_at = " + answered_at + ", "
                "updated_at = NOW()";

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                LOG(LogLevel::ERROR)
                    << "保存作答状态失败: " << mysql_error(_mysql);

                mysql_query(_mysql, "ROLLBACK");
                return false;
            }
        }

        if (mysql_query(_mysql, "COMMIT") != 0)
        {
            LOG(LogLevel::ERROR)
                << "提交保存事务失败: " << mysql_error(_mysql);

            mysql_query(_mysql, "ROLLBACK");
            return false;
        }

        return true;
    }
    catch (const std::exception &)
    {
        mysql_query(_mysql, "ROLLBACK");
        return false;
    }
}
};
