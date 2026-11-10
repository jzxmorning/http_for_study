#include <cstdint>
#include <ctime>
#include "../Comm/userandquestions.hpp"
#include "../Comm/session.hpp"
#include <cassert>
#include <iostream>
#include <regex>
#include <thread>
#include <unordered_set>
#include <vector>

int main()
{
    Sessionid manager;
    auto user = std::make_shared<Session>("student", "student", LOGED, 42, "测试用户");
    assert(user->GetUserId() == 42 && user->GetName() == "测试用户");
    assert(!manager.FindSession("") && !manager.FindSession("forged"));

    std::unordered_set<std::string> ids;
    for (int i = 0; i < 100; ++i)
    {
        auto id = manager.AddSession(user);
        assert(std::regex_match(id, std::regex("[0-9a-f]{64}")));
        assert(ids.insert(id).second);
        assert(manager.FindSession(id) == user);
        manager.RemoveSession(id);
        assert(!manager.FindSession(id));
    }
    manager.RemoveSession("missing");

    bool rejected = false;
    try { manager.AddSession(nullptr); }
    catch (const std::invalid_argument &) { rejected = true; }
    assert(rejected);

    // 短有效期验证真实过期行为，不改变生产环境的 30 分钟期限。
    Sessionid short_lived(std::chrono::seconds(1));
    auto id = short_lived.AddSession(user);
    assert(short_lived.FindSession(id));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    assert(!short_lived.FindSession(id));

    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i)
    {
        workers.emplace_back([&] {
            for (int n = 0; n < 100; ++n)
            {
                auto key = manager.AddSession(user);
                auto found = manager.FindSession(key);
                assert(found == user);
                found->SetStatus(PRACTICE);
                assert(found->GetStatus() == PRACTICE);
                manager.RemoveSession(key);
                assert(!manager.FindSession(key));
            }
        });
    }
    for (auto &worker : workers) worker.join();
    std::cout << "Session: random IDs, expiry, removal and concurrent access passed\n";
}
