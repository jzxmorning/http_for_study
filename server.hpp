#pragma once
#include "Log.hpp"
#include <vector>
#include <iostream>
#include <signal.h>
#include <condition_variable>
#include <functional>
#include <assert.h>
#include <mutex>
#include <cstring>
#include <string>
#include <algorithm>
#include <sys/eventfd.h>
#include <unordered_map>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#define DEFAULT_BUFFER_SIZE 1024
using namespace LogMoudle;
class Buffer
{
private:
    std::vector<char> _buf; // 读写的容器；
    uint64_t _read_index;   // 读指针；
    uint64_t _write_index;  // 写指针；
public:
    Buffer()
        : _buf(DEFAULT_BUFFER_SIZE), _read_index(0), _write_index(0)
    {
    }
    char *Begin()
    {
        return &(*_buf.begin());
    }
    char *WritePosition()
    {
        return Begin() + _write_index;
    }
    char *ReadPosition()
    {
        return Begin() + _read_index;
    }
    uint64_t HeadIdleSize()
    {
        return _read_index;
    }
    uint64_t TailIdleSize()
    {
        return _buf.size() - _write_index;
    }
    uint64_t ReadAbleSize()
    {
        return _write_index - _read_index;
    }
    void MoveReadOffset(uint64_t len)
    {
        if (len == 0)
        {
            return;
        }
        assert(len <= ReadAbleSize());
        _read_index += len;
    }
    void MoveWriteOffset(uint64_t len)
    {
        assert(TailIdleSize() >= len);
        _write_index += len;
    }
    void EnsureWriteSpace(uint64_t len)
    {
        // 后面的内存本来就足够。
        if (len <= TailIdleSize())
        {
            return;
        }
        // 前面的空间加上后面的空间足够。
        if (len <= HeadIdleSize() + TailIdleSize())
        {
            uint64_t read = ReadAbleSize();
            std::copy(ReadPosition(), ReadPosition() + read, Begin());
            _read_index = 0;
            _write_index = read;
        }
        else
        {
            // 空间的确是不够了，需要我们进行扩容。
            _buf.resize(_write_index + len);
        }
    }
    void Write(const void *data, uint64_t len)
    {
        EnsureWriteSpace(len);
        const char *str = (const char *)data;
        std::copy(str, str + len, WritePosition());
    }
    void WriteAndPush(const void *data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }
    void WriteString(const std::string &data)
    {
        const char *d = data.c_str();
        int len = data.size();
        Write(d, len);
    }
    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }
    void WriteBuffer(Buffer &data)
    {
        Write(data.ReadPosition(), data.ReadAbleSize());
    }
    void WriteBufferAndPush(Buffer &data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadAbleSize());
    }
    void Read(void *buf, uint64_t len)
    {
        assert(ReadAbleSize() >= len);
        std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
    }
    void ReadAndPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadOffset(len);
    }
    std::string ReadAsString(uint64_t len)
    {
        assert(ReadAbleSize() >= len);
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    std::string ReadAsStringAndPop(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str = ReadAsString(len);
        MoveReadOffset(len);
        return str;
    }
    char *FindCRLF()
    {
        char *ret = (char *)memchr(ReadPosition(), '\n', ReadAbleSize());
        return ret;
    }
    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        else
        {
            return ReadAsString(pos - ReadPosition() + 1);
        }
    }
    std::string GetLineAndPop()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        else
        {
            std::string str = ReadAsString(pos - ReadPosition() + 1);
            MoveReadOffset(pos - ReadPosition() + 1);
            return str;
        }
    }
    void Clear()
    {
        _read_index = 0;
        _write_index = 0;
    }
};

#define MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket()
        : _sockfd(-1)
    {
    }
    Socket(int fd)
        : _sockfd(fd)
    {
    }
    int Fd()
    {
        return _sockfd;
    }
    ~Socket()
    {
        Close();
    }
    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0)
        {
            LOG(LogLevel::ERROR) << "socket create error";
            return false;
        }
        return true;
    }
    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(addr);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            LOG(LogLevel::ERROR) << "bind error";
            return false;
        }
        return true;
    }
    bool Listen(int backlog = MAX_LISTEN)
    {
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            LOG(LogLevel::ERROR) << "listen error";
            return false;
        }
        return true;
    }
    int Accept()
    {
        int fd = accept(_sockfd, nullptr, nullptr);
        if (fd < 0)
        {
            LOG(LogLevel::ERROR) << "accept error";
            return -1;
        }
        return fd;
    }
    bool Connect(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(addr);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            LOG(LogLevel::ERROR) << "connect error";
            return false;
        }
        return true;
    }
    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        ssize_t n;
        do
        {
            n = recv(_sockfd, buf, len, flag);
        } while (n < 0 && errno == EINTR);
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOG(LogLevel::ERROR) << "recv error";
        }
        // 阻塞客户端用 0 识别正常断开，用 -1 识别读取失败。
        return n;
    }
    ssize_t NoBlockRcv(void *buf, size_t len)
    {
        ssize_t n = Recv(buf, len, MSG_DONTWAIT);
        // 保持事件循环原有约定：0 表示暂时无数据，-1 表示连接结束。
        if (n == 0)
            return -1;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;
        return n;
    }
    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            else
            {
                LOG(LogLevel::ERROR) << "send error";
                return -1;
            }
        }
        return ret;
    }
    ssize_t NoBlockSend(const void *buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT);
    }
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        if (Create() == false)
        {
            return false;
        }
        if (block_flag)
            NonBlock();
        ReuseAddress();
        if (Bind(ip, port) == false)
        {
            return false;
        }
        if (Listen() == false)
        {
            return false;
        }
        return true;
    }
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        if (Create() == false)
        {
            return false;
        }
        if (Connect(ip, port) == false)
        {
            return false;
        }
        return true;
    }
    // 设置套接字选项---开启地址端口重用
    void ReuseAddress()
    {
        // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }
    void NonBlock()
    {
        // int fcntl(int fd, int cmd, ... /* arg */ );
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
};
class EventLoop;
class Channel
{
public:
    Channel(int fd, EventLoop *loop)
        : _fd(fd), _events(0), _revents(0), _loop(loop)
    {
    }
    int Fd()
    {
        return _fd;
    }
    uint32_t Event()
    {
        return _events;
    }
    void SetRevent(uint32_t event)
    {
        _revents = event;
    }
    bool EventReadable()
    {
        return (_events & EPOLLIN);
    }
    bool EventWriteable()
    {
        return (_events & EPOLLOUT);
    }
    void SetReadEvent()
    {
        _events |= EPOLLIN;
        Update();
    }
    void SetWriteEvent()
    {
        _events |= EPOLLOUT;
        Update();
    }
    void RevReadEvent()
    {
        _events = _events & (~EPOLLIN);
        Update();
    }
    void RevWriteEvent()
    {
        _events = _events & (~EPOLLOUT);
        Update();
    }
    void RevAll()
    {
        _events = 0;
        Update();
    }
    void Update();
    void Rev();

private:
    EventLoop *_loop;
    int _fd;
    uint32_t _events;  // 需要监控的事件；
    uint32_t _revents; // 就绪的事件；
    using EventCallBack = std::function<void()>;
    EventCallBack ReadCallBack;   // 读事件就绪回调函数；
    EventCallBack WriteCallBack;  // 写事件就绪回调函数
    EventCallBack ErrorCallBack;  // 错误事件就绪回调函数；
    EventCallBack CloseCallBack;  // 关闭事件就绪回调函数；
    EventCallBack _EventCallBack; // 任意事件就绪回调函数；

public:
    void SetReadCallBack(const EventCallBack &cb)
    {
        ReadCallBack = cb;
    }
    void SetWriteCallBack(const EventCallBack &cb)
    {
        WriteCallBack = cb;
    }
    void SetErrorCallBack(const EventCallBack &cb)
    {
        ErrorCallBack = cb;
    }
    void SetCloseCallBack(const EventCallBack &cb)
    {
        CloseCallBack = cb;
    }
    void SetEvevtCallBack(const EventCallBack &cb)
    {
        _EventCallBack = cb;
    }
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (ReadCallBack)
            {
                ReadCallBack();
            }
        }
        // 因为可能会释放连接所以只能执行一个。
        if (_revents & EPOLLOUT)
        {
            if (WriteCallBack)
            {
                WriteCallBack();
            }
        }
        else if (_revents & EPOLLERR)
        {
            if (ErrorCallBack)
            {
                ErrorCallBack(); // 在这样可能会直接释放连接；
            }
        }
        else if (_revents & EPOLLHUP)
        {
            if (CloseCallBack)
            {
                CloseCallBack();
            }
        }
        // 都要进行的回调函数。
        if (_EventCallBack)
        {
            _EventCallBack();
        }
    }
};

#define MAX_EPOLLEVENT 1024
class Poller
{
public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENT);
        if (_epfd < 0)
        {
            LOG(LogLevel::ERROR) << "epoll_create error";
            abort();
        }
    }
    void Update(Channel *channel, int op)
    {
        int fd = channel->Fd();
        struct epoll_event env;
        env.data.fd = fd;
        env.events = channel->Event();
        int ret = epoll_ctl(_epfd, op, fd, &env);
        if (ret < 0)
        {
            LOG(LogLevel::ERROR) << "epoll_ctrl error";
        }
        return;
    }
    bool HasChannel(Channel *target)
    {
        int fd = target->Fd();
        auto it = _channel.find(fd);
        if (it == _channel.end())
        {
            return false;
        }
        return true;
    }
    void UpdateEvent(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret == true)
        {
            // 修改事件监控
            return Update(channel, EPOLL_CTL_MOD);
        }
        else
        {
            _channel.insert({channel->Fd(), channel});
            return Update(channel, EPOLL_CTL_ADD);
        }
    }
    void RevEvent(Channel *channel)
    {
        auto it = _channel.find(channel->Fd());
        if (it != _channel.end())
        {
            _channel.erase(it);
        }
        Update(channel, EPOLL_CTL_DEL);
    }
    void Poll(std::vector<Channel *> *active)
    {
        int nfd = epoll_wait(_epfd, _env, MAX_EPOLLEVENT, -1);
        if (nfd < 0)
        {
            LOG(LogLevel::ERROR) << "epoll_wait error";
            abort();
        }
        for (int i = 0; i < nfd; i++)
        {
            auto it = _channel.find(_env[i].data.fd);
            assert(it != _channel.end());
            it->second->SetRevent(_env[i].events);
            active->push_back(it->second);
        }
        return;
    }

private:
    int _epfd;
    struct epoll_event _env[MAX_EPOLLEVENT];
    std::unordered_map<int, Channel *> _channel;
};

using TaskFun = std::function<void()>;
using ReleaseFun = std::function<void()>;
class Task
{
public:
    Task(uint64_t id, uint32_t timeout, const TaskFun &fun)
        : _id(id), _timeout(timeout), _func(fun), _iscancel(false)
    {
    }
    ~Task()
    {
        if (_iscancel == false)
        {
            _func();
        }
        _release();
    }
    void Setcancel()
    {
        _iscancel = true;
    }
    void SetRelease(const ReleaseFun &cb)
    {
        _release = cb;
    }
    uint32_t Timeout()
    {
        return _timeout;
    }

private:
    uint64_t _id;        // 任务对象的ID；
    uint32_t _timeout;   // 时间超限的设置；
    bool _iscancel;      // 是否取消任务的执行；
    TaskFun _func;       // 任务的回调函数；
    ReleaseFun _release; // 清除在时间轮中的信息；
};

class TimeWhell
{
private:
    void RvmTimer(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it != _timer.end())
        {
            _timer.erase(it);
        }
    }
    void addtimewhellInLoop(uint64_t id, uint32_t timeout, const TaskFun &fun)
    {
        TaskPtr p(new Task(id, timeout, fun));
        p->SetRelease(std::bind(&TimeWhell::RvmTimer, this, id));
        _timer[id] = WeakPtr(p);
        int pos = (ticket + timeout) % capacity;
        _Timewhell[pos].push_back(p);
    }
    void flushInLoop(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it != _timer.end())
        {
            TaskPtr p1 = it->second.lock();
            if (p1)
            {
                int pos = (ticket + p1->Timeout()) % capacity;
                _Timewhell[pos].push_back(p1);
            }
        }
        return;
    }
    void cancelInLoop(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it != _timer.end())
        {
            TaskPtr p = it->second.lock();
            if (p)
            {
                p->Setcancel();
            }
        }
        return;
    }
    void Run()
    {
        ticket = (ticket + 1) % capacity;
        _Timewhell[ticket].clear();
    }

    static int Createtimefd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            LOG(LogLevel::ERROR) << "create timefd error";
            return -1;
        }
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0;
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }
    int Readtimefd()
    {
        uint64_t times;
        int ret = read(_timefd, &times, sizeof(times));
        if (ret < 0)
        {
            LOG(LogLevel::ERROR) << "read error";
            abort();
        }
        return times;
    }

    void OnTime()
    {
        int ret = Readtimefd();
        for (int i = 0; i < ret; i++)
        {
            Run();
        }
    }

private:
    using TaskPtr = std::shared_ptr<Task>;
    using WeakPtr = std::weak_ptr<Task>;
    int ticket;   // 指针;
    int capacity; // 大小
    std::vector<std::vector<TaskPtr>> _Timewhell;
    std::unordered_map<uint64_t, WeakPtr> _timer;

    EventLoop *loop;
    int _timefd;
    Channel _timer_channel;

public:
    TimeWhell(EventLoop *loop)
        : ticket(0), capacity(60), _Timewhell(capacity), loop(loop), _timefd(Createtimefd()), _timer_channel(_timefd, loop)
    {
        _timer_channel.SetReadCallBack(std::bind(&TimeWhell::OnTime, this));
        _timer_channel.SetReadEvent();
    }
    bool HasTimer(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it != _timer.end())
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    void cancel(uint64_t id);
    void addtimewhell(uint64_t id, uint32_t timeout, const TaskFun &fun);
    void flush(uint64_t id);
};

class EventLoop
{
public:
    void RunTask()
    {
        std::vector<Functor> func;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            std::swap(func, _runv);
        }
        for (auto &V : func)
        {
            V();
        }
        return;
    }
    void ReadEvent()
    {
        uint64_t val;
        int ret = read(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            LOG(LogLevel::ERROR) << "readevent error";
            abort();
        }
        return;
    }
    void WeakUpRun()
    {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            LOG(LogLevel::ERROR) << "weakup error";
            abort();
        }
        return;
    }
    void Start()
    {
        while (1)
        {
            std::vector<Channel *> active;
            _poll.Poll(&active);
            for (auto &V : active)
            {
                V->HandleEvent();
            }
            RunTask();
        }
    }

private:
    using Functor = std::function<void()>;
    std::thread::id _thread_id;
    int _event_fd;
    Channel _event_channel;
    std::vector<Functor> _runv;
    Poller _poll;
    std::mutex _mutex;
    TimeWhell _time_whell;

public:
    void QueueInLoop(const Functor &cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _runv.push_back(cb);
        }
        WeakUpRun();
    }
    bool IsInLoop()
    {
        return (_thread_id == std::this_thread::get_id());
    }
    void AssertInLoop()
    {
        assert(IsInLoop());
    }
    void RunInLoop(const Functor &cb)
    {
        if (IsInLoop())
        {
            cb();
        }
        else
        {
            QueueInLoop(cb);
        }
    }
    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                LOG(LogLevel::ERROR) << "eventfd error";
                abort();
            }
        }
        return efd;
    }
    EventLoop()
        : _thread_id(std::this_thread::get_id()), _event_fd(CreateEventFd()), _event_channel(_event_fd, this), _time_whell(this)
    {
        _event_channel.SetReadCallBack(std::bind(&EventLoop::ReadEvent, this));
        _event_channel.SetReadEvent();
    }
    void EventUpdate_eventloop(Channel *channel)
    {
        _poll.UpdateEvent(channel);
    }
    void EventRev_eventloop(Channel *channel)
    {
        _poll.RevEvent(channel);
    }
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFun &cb)
    {
        _time_whell.addtimewhell(id, delay, cb);
    }
    void TimerRefesh(uint64_t id)
    {
        _time_whell.flush(id);
    }
    void TimerCancel(uint64_t id)
    {
        _time_whell.cancel(id);
    }
    bool HasTimer(uint64_t id)
    {
        return _time_whell.HasTimer(id);
    }
};

class Any
{
private:
    class holder
    {
    public:
        virtual ~holder() {}
        virtual const std::type_info &type() = 0;
        virtual holder *clone() = 0;
    };
    template <class T>
    class placeholder : public holder
    {
    public:
        T _val;

    public:
        placeholder(const T &val)
        {
            _val = val;
        }
        virtual holder *clone()
        {
            return (new placeholder(_val));
        }
        virtual const std::type_info &type()
        {
            return typeid(T);
        }
    };
    holder *_content;

public:
    Any()
    {
        _content = NULL;
    }
    template <class T>
    Any(const T &val)
    {
        _content = new placeholder<T>(val);
    }
    Any(const Any &other)
    {
        if (other._content == NULL)
        {
            _content = NULL;
        }
        else
        {
            _content = other._content->clone();
        }
    }
    ~Any()
    {
        delete _content;
    }
    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }
    template <class T>
    T *get()
    {
        assert(typeid(T) == _content->type());
        return &((placeholder<T> *)_content)->_val;
    }
    template <class T>
    Any &operator=(const T &val)
    {
        Any temp(val);
        temp.swap(*this);
        return *this;
    }
    Any &operator=(const Any &other)
    {
        Any(other).swap(*this);
        return *this;
    }
};

class Connection;
typedef enum
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
} ConnStatu;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    uint64_t _con_id;
    int _socketfd;
    Socket _sock;
    bool _enable_inactive_release;
    EventLoop *_loop;
    Channel _channel;
    Buffer _inbuffer;
    Buffer _outbuffer;
    ConnStatu _statu;
    Any _content;
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _event_callback;
    ClosedCallback _ser_closed_callback;

private:
    void HandleRead()
    {
        char buf[65525];
        ssize_t ret = _sock.NoBlockRcv(buf, sizeof(buf));
        if (ret < 0)
        {
            if (_inbuffer.ReadAbleSize() > 0)
            {
                _message_callback(shared_from_this(), &_inbuffer);
            }
            return Release();
        }
        _inbuffer.WriteAndPush(buf, ret);
        if (_inbuffer.ReadAbleSize() > 0)
        {
            return _message_callback(shared_from_this(), &_inbuffer);
        }
    }
    void HandleWrite()
    {
        ssize_t ret = _sock.NoBlockSend(_outbuffer.ReadPosition(), _outbuffer.ReadAbleSize());
        if (ret < 0)
        {
            if (_inbuffer.ReadAbleSize() > 0)
            {
                _message_callback(shared_from_this(), &_inbuffer);
            }
            return Release();
        }
        _outbuffer.MoveReadOffset(ret);
        if (_outbuffer.ReadAbleSize() == 0)
        {
            _channel.RevWriteEvent();
        }
        if (_statu == DISCONNECTING && _outbuffer.ReadAbleSize() == 0)
        {
            return Release();
        }
    }
    void HandleClose()
    {
        if (_inbuffer.ReadAbleSize() > 0)
        {
            _message_callback(shared_from_this(), &_inbuffer);
        }
        return Release();
    }
    void HandleError()
    {
        return HandleClose();
    }
    void HandleEvent()
    {
        if (_enable_inactive_release == true)
        {
            _loop->TimerRefesh(_con_id);
        }
        if (_event_callback)
        {
            _event_callback(shared_from_this());
        }
    }
    void EstablishedInloop()
    {
        assert(_statu == CONNECTING);
        _statu = CONNECTED;
        _channel.SetReadEvent();
        if (_connected_callback)
        {
            _connected_callback(shared_from_this());
        }
    }
    void ReleaseInLoop()
    {
        if (_statu == DISCONNECTED)
            return;
        _statu = DISCONNECTED;
        _channel.Rev();
        _sock.Close();
        if (_loop->HasTimer(_con_id))
        {
            _loop->TimerCancel(_con_id);
        }
        if (_closed_callback)
        {
            _closed_callback(shared_from_this());
        }
        if (_ser_closed_callback)
        {
            _ser_closed_callback(shared_from_this());
        }
    }
    void SendInloop(Buffer &buf)
    {
        if (_statu == DISCONNECTED)
        {
            return;
        }
        _outbuffer.WriteBufferAndPush(buf);
        if (_channel.EventWriteable() == false)
        {
            _channel.SetWriteEvent();
        }
    }
    void ShutdownInLoop()
    {
        _statu = DISCONNECTING;
        if (_inbuffer.ReadAbleSize() > 0)
        {
            _message_callback(shared_from_this(), &_inbuffer);
        }
        if (_outbuffer.ReadAbleSize() > 0)
        {
            if (_channel.EventWriteable() == false)
            {
                _channel.SetWriteEvent();
            }
        }
        if (_outbuffer.ReadAbleSize() == 0)
        {
            Release();
        }
    }

    void EnableInactiveReleaseInLoop(int sec)
    {
        _enable_inactive_release = true;
        if (_loop->HasTimer(_con_id) == true)
        {
            return _loop->TimerRefesh(_con_id);
        }
        _loop->TimerAdd(_con_id, sec, std::bind(&Connection::Release, this));
    }

    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;
        if (_loop->HasTimer(_con_id) == true)
        {
            _loop->TimerCancel(_con_id);
        }
    }
    void UpgradeInLoop(const Any &context,
                       const ConnectedCallback &conn,
                       const MessageCallback &msg,
                       const ClosedCallback &closed,
                       const AnyEventCallback &event)
    {
        _content = context;
        _connected_callback = conn;
        _message_callback = msg;
        _closed_callback = closed;
        _event_callback = event;
    }

public:
    Connection(int con_id, int socketfd, EventLoop *loop)
        : _channel(_socketfd, loop), _sock(socketfd), _loop(loop), _socketfd(socketfd), _statu(CONNECTING), _enable_inactive_release(false), _con_id(con_id)
    {
        _channel.SetCloseCallBack(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallBack(std::bind(&Connection::HandleError, this));
        _channel.SetReadCallBack(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite, this));
        _channel.SetEvevtCallBack(std::bind(&Connection::HandleEvent, this));
    }
    ~Connection()
    {
    }
    int Fd() { return _socketfd; }
    // 获取连接ID
    int Id() { return _con_id; }
    // 是否处于CONNECTED状态
    bool Connected() { return (_statu == CONNECTED); }
    // 设置上下文--连接建立完成时进行调用
    void SetContext(const Any &context) { _content = context; }
    // 获取上下文，返回的是指针
    Any *GetContext() { return &_content; }
    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_callback = cb; }
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }
    void SetAnyEventCallback(const AnyEventCallback &cb) { _event_callback = cb; }
    void SetSrvClosedCallback(const ClosedCallback &cb) { _ser_closed_callback = cb; }
    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInloop, this));
    }
    void Send(const char *data, size_t len)
    {
        Buffer buf;
        buf.WriteAndPush(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInloop, this, buf));
    }
    void Shutdown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }

    void Release()
    {
        // 当前事件回调结束后再释放，任务持有连接，避免回调访问已析构的对象。
        _loop->QueueInLoop([self = shared_from_this()] {
            self->ReleaseInLoop();
        });
    }
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }
    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }
    void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg,
                 const ClosedCallback &closed, const AnyEventCallback &event)
    {
        assert(_loop->IsInLoop());
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn, msg, closed, event));
    }
};
void Channel::Update()
{
    _loop->EventUpdate_eventloop(this);
}
void Channel::Rev()
{
    _loop->EventRev_eventloop(this);
}
void TimeWhell::cancel(uint64_t id)
{
    loop->RunInLoop(std::bind(&TimeWhell::cancelInLoop, this, id));
}
void TimeWhell::addtimewhell(uint64_t id, uint32_t timeout, const TaskFun &fun)
{
    loop->RunInLoop(std::bind(&TimeWhell::addtimewhellInLoop, this, id, timeout, fun));
}
void TimeWhell::flush(uint64_t id)
{
    loop->RunInLoop(std::bind(&TimeWhell::flushInLoop, this, id));
}

class Acceptor
{
private:
    Socket _sock;
    EventLoop *_loop;
    Channel _channel;
    using AcceptCallBack = std::function<void(int)>;
    AcceptCallBack _acceptcallback;

private:
    void HandleRead()
    {
        int newfd = _sock.Accept();
        if (newfd < 0)
        {
            LOG(LogLevel::ERROR) << "accept error";
            return;
        }
        if (_acceptcallback)
        {
            _acceptcallback(newfd);
        }
    }
    int CreateServer(int port)
    {
        bool ret = _sock.CreateServer(port, "0.0.0.0", true);
        if (ret == false)
        {
            LOG(LogLevel::ERROR) << "create listening socket error";
            abort();
        }
        return _sock.Fd();
    }

public:
    Acceptor(EventLoop *loop, int port)
        : _sock(), _loop(loop), _channel(CreateServer(port), loop)
    {
        _channel.SetReadCallBack(std::bind(&Acceptor::HandleRead, this));
    }
    void Listen()
    {
        _channel.SetReadEvent();
    }
    void SetAcceptCallBack(const AcceptCallBack &cb)
    {
        _acceptcallback = cb;
    }
};

class LoopThread
{
private:
    std::mutex _mutex;
    std::condition_variable _cond;
    EventLoop *_loop;
    std::thread _thread;

private:
    void ThreadEntry()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _loop = &loop;
            _cond.notify_all();
        }
        loop.Start();
    }

public:
    LoopThread()
        : _loop(NULL), _thread(std::thread(&LoopThread::ThreadEntry, this))
    {
    }
    EventLoop *Get()
    {
        EventLoop *loop = NULL;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _cond.wait(_lock, [&]()
                       { return _loop != NULL; });
            loop = _loop;
        }
        return loop;
    }
};

class LoopThreadPool
{
private:
    int _thread_count;
    int _next_id;
    std::vector<LoopThread *> _threads;
    std::vector<EventLoop *> _loops;
    EventLoop *_basicloop;

public:
    LoopThreadPool(EventLoop *basicloop)
        : _basicloop(basicloop), _thread_count(0), _next_id(0)
    {
    }
    void Setthreadcount(int n)
    {
        _thread_count = n;
    }
    void Create()
    {
        if (_thread_count > 0)
        {
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);
            for (int i = 0; i < _thread_count; i++)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->Get();
            }
        }
        return;
    }
    EventLoop *Next()
    {
        if (_thread_count == 0)
        {
            return _basicloop;
        }
        _next_id = (_next_id + 1) % _thread_count;
        return _loops[_next_id];
    }
};

class TcpServer
{
private:
    uint64_t _next_id;
    int _port;
    int _timeout;
    bool _enable_inactive_release;
    EventLoop _basicloop;
    LoopThreadPool _loops;
    std::unordered_map<uint64_t, PtrConnection> _connections;
    Acceptor _acceptor;
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
    using Functor = std::function<void()>;
    ConnectedCallback _connection_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _event_callback;

private:
    void RunAfterInloop(int delay, Functor &cb)
    {
        _next_id++;
        _basicloop.TimerAdd(_next_id, delay, cb);
    }
    void NewConnection(int fd)
    {
        _next_id++;
        PtrConnection conn(new Connection(_next_id, fd, _loops.Next()));
        conn->SetMessageCallback(_message_callback);
        conn->SetConnectedCallback(_connection_callback);
        conn->SetAnyEventCallback(_event_callback);
        conn->SetClosedCallback(_closed_callback);
        conn->SetSrvClosedCallback(std::bind(&TcpServer::EraseConnection, this, std::placeholders::_1));
        if (_enable_inactive_release == true)
        {
            conn->EnableInactiveRelease(_timeout);
        }
        conn->Established();
        _connections.insert({_next_id, conn});
    }
    void EraseConnectionInLoop(const PtrConnection &conn)
    {
        int id = conn->Id();
        auto it = _connections.find(id);
        if (it != _connections.end())
        {
            _connections.erase(it);
        }
    }
    void EraseConnection(const PtrConnection &conn)
    {
        _basicloop.RunInLoop(std::bind(&TcpServer::EraseConnectionInLoop, this, conn));
    }

public:
    TcpServer(int port)
        : _next_id(0), _port(port), _timeout(0), _enable_inactive_release(false),
          _basicloop(), _loops(&_basicloop), _acceptor(&_basicloop, _port)
    {
        _acceptor.SetAcceptCallBack(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        _acceptor.Listen();
    }

    void SetThreadCount(int count)
    {
        _loops.Setthreadcount(count);
    }
    void SetMessageCallback(const MessageCallback &cb)
    {
        _message_callback = cb;
    }
    void SetConnectedCallback(const ConnectedCallback &cb)
    {
        _connection_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb)
    {
        _event_callback = cb;
    }
    void SetClosedCallback(const ClosedCallback &cb)
    {
        _closed_callback = cb;
    }
    void RunAfter(int delay, Functor &cb)
    {
        _basicloop.RunInLoop(std::bind(&TcpServer::RunAfterInloop, this, delay, cb));
    }
    void EnableInactiveRelease(int timeout)
    {
        _timeout = timeout;
        _enable_inactive_release = true;
    }
    void Start()
    {
        _loops.Create();
        _basicloop.Start();
    }
};

class NetWork
{
public:
    NetWork()
    {
        // DBG_LOG("SIGPIPE INIT");
        LOG(LogLevel::DEBUG) << "SIGPIPE INIT";
        signal(SIGPIPE, SIG_IGN);
    }
};
static NetWork nw;
