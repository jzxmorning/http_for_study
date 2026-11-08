#pragma once

#include <array>
#include <atomic>
#include<unordered_map>
#include <cerrno>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/random.h>
#include <unordered_map>

enum STATUS { LOGED, LOGING, PRACTICE, EXAMING };
inline constexpr int SESSION_LIFETIME_SECONDS = 1800;

// 身份信息由登录查询提供；Session 不保存密码。
class Session
{
    public:
    const std::string username;
    const std::string identity;
    const int user_id;
    const std::string display_name;
    std::unordered_map<int,QuestionProgress> progress;
    std::atomic<STATUS> status;
    mutable std::mutex progress_mutex;
public:
    Session(const std::string &account, const std::string &role = "student",
            STATUS state = LOGING, int id = 0, const std::string &name = "")
        : username(account), identity(role), user_id(id),
          display_name(name.empty() ? account : name), status(state) {}

    const std::string &GetUsername() const { return username; }
    const std::string &GetIdentity() const { return identity; }
    const std::string &GetName() const { return display_name; }
    int GetUserId() const { return user_id; }
    STATUS GetStatus() const { return status.load(); }
    void SetStatus(STATUS state) { status.store(state); }
    void RecordAnswer(int q_id, const std::string &answer, bool correct)
{
    std::lock_guard<std::mutex> guard(progress_mutex);

    auto &record = progress[q_id];

    record.question_id = q_id;
    record.last_answer = answer;
    record.last_correct = correct;

    ++record.attempt_count;
    if (correct)
    {
        ++record.correct_count;
    }

    record.last_answer_at = std::time(nullptr);
    record.dirty = true;
    ++record.version;
}
};

using session_ptr = std::shared_ptr<Session>;

class Sessionid
{
    using Clock = std::chrono::steady_clock;
    struct Entry
    {
        session_ptr session;
        Clock::time_point expires_at;
    };

    std::unordered_map<std::string, Entry> sessions;
    std::mutex mutex;
    const std::chrono::seconds lifetime;

    static std::string MakeId()
    {
        std::array<unsigned char, 32> bytes{};
        size_t offset = 0;
        while (offset < bytes.size())
        {
            ssize_t n = getrandom(bytes.data() + offset, bytes.size() - offset, 0);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                throw std::runtime_error("生成 Session 编号失败");
            offset += static_cast<size_t>(n);
        }
        const char hex[] = "0123456789abcdef";
        std::string id;
        id.reserve(64);
        for (unsigned char b : bytes)
        {
            id += hex[b >> 4];
            id += hex[b & 15];
        }
        return id;
    }

public:
    explicit Sessionid(std::chrono::seconds ttl = std::chrono::seconds(SESSION_LIFETIME_SECONDS))
        : lifetime(ttl)
    {
        if (ttl.count() <= 0)
            throw std::invalid_argument("Session 有效期必须大于零");
    }

    std::string AddSession(session_ptr session)
    {
        if (!session)
            throw std::invalid_argument("Session 不能为空");
        for (;;)
        {
            std::string id = MakeId();
            std::lock_guard<std::mutex> guard(mutex);
            auto now = Clock::now();
            for (auto it = sessions.begin(); it != sessions.end();)
            {
                if (now >= it->second.expires_at)
                    it = sessions.erase(it);
                else
                    ++it;
            }
            if (sessions.emplace(id, Entry{session, now + lifetime}).second)
                return id;
        }
    }

    session_ptr FindSession(const std::string &id)
    {
        std::lock_guard<std::mutex> guard(mutex);
        auto it = sessions.find(id);
        if (it == sessions.end())
            return nullptr;
        if (Clock::now() >= it->second.expires_at)
        {
            sessions.erase(it);
            return nullptr;
        }
        return it->second.session;
    }

    void RemoveSession(const std::string &id)
    {
        std::lock_guard<std::mutex> guard(mutex);
        sessions.erase(id);
    }
};
