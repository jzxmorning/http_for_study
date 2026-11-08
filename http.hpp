#include <iostream>
#include <string>
#include <fstream>
#include "Log.hpp"
#include <regex>
#include <vector>
#include "server.hpp"
#include <unordered_map>
#include <sys/stat.h>
#include <sys/types.h>
using namespace LogMoudle;
std::unordered_map<int, std::string> _statu_msg =
    {
        {100, "Continue"},
        {101, "Switching Protocol"},
        {102, "Processing"},
        {103, "Early Hints"},
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {207, "Multi-Status"},
        {208, "Already Reported"},
        {226, "IM Used"},
        {300, "Multiple Choice"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {306, "unused"},
        {307, "Temporary Redirect"},
        {308, "Permanent Redirect"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {418, "I'm a teapot"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Entity"},
        {423, "Locked"},
        {424, "Failed Dependency"},
        {425, "Too Early"},
        {426, "Upgrade Required"},
        {428, "Precondition Required"},
        {429, "Too Many Requests"},
        {431, "Request Header Fields Too Large"},
        {451, "Unavailable For Legal Reasons"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
        {506, "Variant Also Negotiates"},
        {507, "Insufficient Storage"},
        {508, "Loop Detected"},
        {510, "Not Extended"},
        {511, "Network Authentication Required"}};
std::unordered_map<std::string, std::string> _mime_msg =
    {
        {".aac", "audio/aac"},
        {".abw", "application/x-abiword"},
        {".arc", "application/x-freearc"},
        {".avi", "video/x-msvideo"},
        {".azw", "application/vnd.amazon.ebook"},
        {".bin", "application/octet-stream"},
        {".bmp", "image/bmp"},
        {".bz", "application/x-bzip"},
        {".bz2", "application/x-bzip2"},
        {".csh", "application/x-csh"},
        {".css", "text/css"},
        {".csv", "text/csv"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".eot", "application/vnd.ms-fontobject"},
        {".epub", "application/epub+zip"},
        {".gif", "image/gif"},
        {".htm", "text/html"},
        {".html", "text/html"},
        {".ico", "image/vnd.microsoft.icon"},
        {".ics", "text/calendar"},
        {".jar", "application/java-archive"},
        {".jpeg", "image/jpeg"},
        {".jpg", "image/jpeg"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".jsonld", "application/ld+json"},
        {".mid", "audio/midi"},
        {".midi", "audio/x-midi"},
        {".mjs", "text/javascript"},
        {".mp3", "audio/mpeg"},
        {".mpeg", "video/mpeg"},
        {".mpkg", "application/vnd.apple.installer+xml"},
        {".odp", "application/vnd.oasis.opendocument.presentation"},
        {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
        {".odt", "application/vnd.oasis.opendocument.text"},
        {".oga", "audio/ogg"},
        {".ogv", "video/ogg"},
        {".ogx", "application/ogg"},
        {".otf", "font/otf"},
        {".png", "image/png"},
        {".pdf", "application/pdf"},
        {".ppt", "application/vnd.ms-powerpoint"},
        {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".rar", "application/x-rar-compressed"},
        {".rtf", "application/rtf"},
        {".sh", "application/x-sh"},
        {".svg", "image/svg+xml"},
        {".swf", "application/x-shockwave-flash"},
        {".tar", "application/x-tar"},
        {".tif", "image/tiff"},
        {".tiff", "image/tiff"},
        {".ttf", "font/ttf"},
        {".txt", "text/plain"},
        {".vsd", "application/vnd.visio"},
        {".wav", "audio/wav"},
        {".weba", "audio/webm"},
        {".webm", "video/webm"},
        {".webp", "image/webp"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".xhtml", "application/xhtml+xml"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".xml", "application/xml"},
        {".xul", "application/vnd.mozilla.xul+xml"},
        {".zip", "application/zip"},
        {".3gp", "video/3gpp"},
        {".3g2", "video/3gpp2"},
        {".7z", "application/x-7z-compressed"}};
class Util
{
public:
    static ssize_t Split(const std::string &str, const std::string &sep, std::vector<std::string> *arry)
    {
        ssize_t offset = 0;
        while (offset < str.size())
        {
            ssize_t pos = str.find(sep, offset);
            if (pos == std::string::npos)
            {
                if (pos == str.size())
                {
                    break;
                }
                arry->push_back(str.substr(offset));
                return arry->size();
            }
            if (pos == offset)
            {
                offset = pos + sep.size();
                continue;
            }
            arry->push_back(str.substr(offset, pos - offset));
            offset = pos + sep.size();
            continue;
        }
        return arry->size();
    }
    static bool ReadFile(const std::string &filename, std::string *buf)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            LOG(LogLevel::ERROR) << "open file error";
            return false;
        }
        ssize_t size = 0;
        ifs.seekg(0, ifs.end);
        size = ifs.tellg();
        buf->resize(size);
        ifs.seekg(0, ifs.beg);
        ifs.read(&(*buf)[0], size);
        if (ifs.good() == false)
        {
            LOG(LogLevel::ERROR) << "read file error";
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }
    static bool WriteFile(const std::string &filename, const std::string &buf)
    {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (ofs.is_open() == false)
        {
            LOG(LogLevel::ERROR) << "open file error";
            return false;
        }
        ofs.write(buf.c_str(), buf.size());
        if (ofs.good() == false)
        {
            LOG(LogLevel::ERROR) << "write file error";
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }
    static std::string UrlEncode(const std::string &url, bool convert_space_to_plus)
    {
        std::string ret;
        for (auto &v : url)
        {
            if (v == '.' || v == '_' || v == '-' || v == '~' || isalnum(v))
            {
                ret += v;
                continue;
            }
            if (v == ' ' && convert_space_to_plus == true)
            {
                ret += '+';
                continue;
            }
            char c[4] = {0};
            snprintf(c, 4, "%%%02x", v);
            ret += c;
        }
        return ret;
    }
    static int HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        return -1;
    }
    static std::string UrlDecode(const std::string &url, bool convert_space_to_plus)
    {
        std::string ret;
        for (int i = 0; i < url.size(); i++)
        {
            if (url[i] == '+' && convert_space_to_plus == true)
            {
                ret += ' ';
                continue;
            }
            if (url[i] == '%' && (i + 2) < url.size())
            {
                int v1 = HEXTOI(url[i + 1]);
                int v2 = HEXTOI(url[i + 2]);
                if (v1 >= 0 && v2 >= 0)
                {
                    ret += static_cast<char>(v1 * 16 + v2);
                    i += 2;
                    continue;
                }
            }
            ret += url[i];
        }
        return ret;
    }
    static std::string StatuDesc(int statu)
    {
        auto it = _statu_msg.find(statu);
        if (it != _statu_msg.end())
        {
            std::string ret = it->second;
            return ret;
        }
        return "Unknow";
    }
    static std::string ExtMime(const std::string &filename)
    {
        int pos = filename.find_last_of('.');
        if (pos == std::string::npos)
        {
            return "application/octet-stream";
        }
        std::string temp = filename.substr(pos);
        auto it = _mime_msg.find(temp);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream";
        }
        return it->second;
    }
    static bool IsDirectory(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }
    static bool IsRegular(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISREG(st.st_mode);
    }
    static bool ValidPath(const std::string &path)
    {
        int level = 0;
        std::vector<std::string> temp;
        Split(path, "/", &temp);
        for (auto &V : temp)
        {
            if (V == "..")
            {
                level--;
                if (level < 0)
                {
                    return false;
                }
                continue;
            }
            level++;
        }
        return true;
    }
};

class HttpResponse
{
public:
    int _statu;
    bool _redirect_flag;
    std::string _body;
    std::string _redirect_url;
    std::unordered_map<std::string, std::string> _headers;

public:
    HttpResponse()
        : _statu(200), _redirect_flag(false)
    {
    }
    HttpResponse(int statu)
        : _statu(statu), _redirect_flag(false)
    {
    }
    void Reset()
    {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert({key, val});
    }
    bool HasHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    void SetContent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    void SetRedirect(const std::string &url, int statu = 302)
    {
        _redirect_flag = true;
        _redirect_url = url;
        _statu = statu;
    }
    bool Close()
    {
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

class HTTPRequest
{
public:
    std::string _body;   // 请求正文
    std::string _method; // 请求方法
    std::string _path;   // 请求资源路径
    std::string _version;
    std::smatch _matches;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _params;

public:
    HTTPRequest()
        : _version("HTTP/1.1")
    {
    }
    void ReSet()
    {
        _body.clear();
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert({key, val});
    }
    bool HasHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    std::string GetHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    void SetContent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    void SetParam(const std::string &key, const std::string &val)
    {
        _params.insert({key, val});
    }
    bool HasParam(const std::string &key)
    {
        auto it = _params.find(key);
        if (it != _params.end())
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    std::string GetParam(const std::string &key)
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return "";
        }
        return it->second;
    }
    size_t ContentLength()
    {
        bool ret = HasHeader("Content-Length");
        if (ret == false)
        {
            return 0;
        }
        std::string temp = GetHeader("Content-Length");
        return std::stol(temp);
    }
    bool Close() const
    {
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

typedef enum
{
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
} HttpRecvStatu;

#define MAX_LINE 8192
class HTTPContext
{
private:
    HttpRecvStatu _rcv_statu; // 解析状态
    int _resp_statu;          // 相应状态码
    HTTPRequest _request;

private:
    bool ParseHttpLine(const std::string &line)
    {
        std::regex res("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP\\/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        std::smatch matches;
        bool ret = std::regex_match(line, matches, res);
        if (ret == false)
        {
            _rcv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400;
            return false;
        }
        // 0 : get /baidu/login?user=xiaoming&pass=123123 HTTP/1.1
        // 1 : get
        // 2 : /baidu/login
        // 3 : user=xiaoming&pass=123123
        // 4 : HTTP/1.1
        _request._method = matches[1];
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper);
        _request._path = Util::UrlDecode(matches[2], false);
        _request._version = matches[4];
        std::vector<std::string> temp;
        Util::Split(matches[3], "&", &temp);
        for (auto &v : temp)
        {
            size_t pos = v.find("=");
            if (pos == std::string::npos)
            {
                _rcv_statu = RECV_HTTP_ERROR;
                _resp_statu = 400;
                return false;
            }
            std::string key = Util::UrlDecode(v.substr(0, pos), true);
            std::string val = Util::UrlDecode(v.substr(pos + 1), true);
            _request.SetParam(key, val);
        }
        return true;
    }
    bool RecvHttpLine(Buffer *buf)
    {
        if (_rcv_statu != RECV_HTTP_LINE)
        {
            return false;
        }
        std::string line = buf->GetLineAndPop();
        if (line.size() == 0)
        {
            if (buf->ReadAbleSize() > MAX_LINE)
            {
                _rcv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414;
                return false;
            }
            return true;
        }
        if (line.size() > MAX_LINE)
        {
            _rcv_statu = RECV_HTTP_ERROR;
            _resp_statu = 414;
            return false;
        }
        bool ret = ParseHttpLine(line);
        if (ret == false)
        {
            return false;
        }
        _rcv_statu = RECV_HTTP_HEAD;
        return true;
    }
    bool ParseHttpHead(std::string &line)
    {
        // key: val\r\nkey: val\r\n....
        if (line.back() == '\n')
        {
            line.pop_back();
        }
        if (line.back() == '\r')
        {
            line.pop_back();
        }
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            _rcv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400;
            return false;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);
        _request.SetHeader(key, val);
        return true;
    }
    bool RecvHttpHead(Buffer *buf)
    {
        if (_rcv_statu != RECV_HTTP_HEAD)
        {
            return false;
        }
        while (1)
        {
            std::string line = buf->GetLineAndPop();
            if (line.size() == 0)
            {
                if (buf->ReadAbleSize() > MAX_LINE)
                {
                    _rcv_statu = RECV_HTTP_ERROR;
                    _resp_statu = 414;
                    return false;
                }
                return true;
            }
            if (line.size() > MAX_LINE)
            {
                _rcv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414;
                return false;
            }
            if (line == "\r\n" || line == "\n")
            {
                break;
            }
            bool ret = ParseHttpHead(line);
            if (ret == false)
            {
                return false;
            }
        }
        _rcv_statu = RECV_HTTP_BODY;
        return true;
    }
    bool RecvHttpBody(Buffer *buf)
    {
        if (_rcv_statu != RECV_HTTP_BODY)
        {
            return false;
        }
        size_t content_length = _request.ContentLength();
        if (content_length == 0)
        {
            _rcv_statu = RECV_HTTP_OVER;
            return true;
        }
        size_t real_length = content_length - _request._body.size();
        if (buf->ReadAbleSize() >= real_length)
        {
            _request._body.append(buf->ReadPosition(), real_length);
            buf->MoveReadOffset(real_length);
            _rcv_statu = RECV_HTTP_OVER;
            return true;
        }
        _request._body.append(buf->ReadPosition(), buf->ReadAbleSize());
        buf->MoveReadOffset(buf->ReadAbleSize());
        return true;
    }

public:
    HTTPContext()
        : _rcv_statu(RECV_HTTP_LINE), _resp_statu(200)
    {
    }
    void Reset()
    {
        _rcv_statu = RECV_HTTP_LINE;
        _resp_statu = 200;
        _request.ReSet();
    }
    int RespStatu()
    {
        return _resp_statu;
    }
    HttpRecvStatu RecvStatu()
    {
        return _rcv_statu;
    }
    HTTPRequest &Request()
    {
        return _request;
    }
    void RecvHttpRequest(Buffer *buf)
    {
        switch (_rcv_statu)
        {
        case RECV_HTTP_LINE:
            RecvHttpLine(buf);
        case RECV_HTTP_HEAD:
            RecvHttpHead(buf);
        case RECV_HTTP_BODY:
            RecvHttpBody(buf);
        }
        return;
    }
};

#define DEFALT_TIMEOUT 10
class HttpServer
{
private:
    using Handler = std::function<void(const HTTPRequest &, HttpResponse *)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;
    std::string _basedir;
    TcpServer _tcpserver;

private:
    void ErrorHandler(const HTTPRequest &req, HttpResponse *rsp)
    {
        // 1. 组织一个错误展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(rsp->_statu);
        body += " ";
        body += Util::StatuDesc(rsp->_statu);
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        // 2. 将页面数据，当作响应正文，放入rsp中
        rsp->SetContent(body, "text/html");
    }
    void WriteReponse(const PtrConnection &conn, const HTTPRequest &req, HttpResponse &rsp)
    {
        if (req.Close() == true)
        {
            rsp.SetHeader("Connection", "close");
        }
        else
        {
            rsp.SetHeader("Connection", "keep-alive");
        }
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Length") == false)
        {
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
        }
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Type") == false)
        {
            rsp.SetHeader("Content-Type", "application/octet-stream");
        }
        if (rsp._redirect_flag == true)
        {
            rsp.SetHeader("Location", rsp._redirect_url);
        }
        std::stringstream rsp_str;
        rsp_str << req._version << " " << std::to_string(rsp._statu) << " " << Util::StatuDesc(rsp._statu) << "\r\n";
        for (auto &head : rsp._headers)
        {
            rsp_str << head.first << ": " << head.second << "\r\n";
        }
        rsp_str << "\r\n";
        rsp_str << rsp._body;
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }
    bool IsFileHandler(const HTTPRequest &req)
    {
        if (_basedir.empty() == true)
        {
            return false;
        }
        if (req._method != "GET" && req._method != "POST")
        {
            return false;
        }
        if (Util::ValidPath(req._path) == false)
        {
            return false;
        }
        std::string req_path = _basedir + req._path;
        if (req._path.back() == '/')
        {
            req_path.append("index.html");
        }
        if (Util::IsRegular(req_path) == false)
        {
            return false;
        }
        return true;
    }
    void FileHandler(const HTTPRequest &req, HttpResponse *rsp)
    {
        std::string req_path = _basedir + req._path;
        if (req_path.back() == '/')
        {
            req_path += "index.html";
        }
        bool ret = Util::ReadFile(req_path, &rsp->_body);
        if (ret == false)
        {
            return;
        }
        std::string mime = Util::ExtMime(req_path);
        rsp->SetHeader("Content-Type", mime);
        return;
    }
    void Dispatcher(HTTPRequest &req, HttpResponse *rsp, Handlers &handlers)
    {
        for (auto &head : handlers)
        {
            std::regex re = head.first;
            Handler Functor = head.second;
            bool ret = std::regex_match(req._path, req._matches, re);
            if (ret == false)
            {
                continue;
            }
            return Functor(req, rsp);
        }
        rsp->_statu = 404;
        return;
    }
    void Route(HTTPRequest &req, HttpResponse *rsp)
    {
        if (IsFileHandler(req) == true)
        {
            return FileHandler(req, rsp);
        }
        if (req._method == "GET")
        {
            return Dispatcher(req, rsp, _get_route);
        }
        else if (req._method == "POST")
        {
            return Dispatcher(req, rsp, _post_route);
        }
        else if (req._method == "PUT")
        {
            return Dispatcher(req, rsp, _put_route);
        }
        else if (req._method == "DELETE")
        {
            return Dispatcher(req, rsp, _delete_route);
        }
        rsp->_statu = 405; // Method Not Allowed
        return;
    }
    void OnConnected(const PtrConnection &conn)
    {
        conn->SetContext(HTTPContext());
        LOG(LogLevel::DEBUG) << "NEW CONNECTION" << conn.get();
    }
    void OnMessage(const PtrConnection &conn, Buffer *buffer)
    {
        while (buffer->ReadAbleSize() > 0)
        {
            HTTPContext *context = conn->GetContext()->get<HTTPContext>();
            context->RecvHttpRequest(buffer);
            HTTPRequest request = context->Request();
            HttpResponse response(context->RespStatu());
            if (context->RespStatu() >= 400)
            {
                ErrorHandler(request, &response);
                WriteReponse(conn, request, response);
                context->Reset();
                buffer->MoveReadOffset(buffer->ReadAbleSize()); // 出错了就把缓冲区数据清空
                conn->Shutdown();
                return;
            }
            if (context->RecvStatu() != RECV_HTTP_OVER)
            {
                return;
            }
            Route(request, &response);
            WriteReponse(conn, request, response);
            context->Reset();
            if (request.Close() == true)
            {
                conn->Shutdown();
            }
        }
    }

public:
    HttpServer(int port, int timeout = DEFALT_TIMEOUT)
        : _tcpserver(port)
    {
        _tcpserver.EnableInactiveRelease(timeout);
        _tcpserver.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _tcpserver.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    void SetBaseDir(const std::string &path)
    {
        assert(Util::IsDirectory(path) == true);
        _basedir = path;
    }
    void Get(const std::string &pattern, const Handler &handler)
    {
        _get_route.push_back({std::regex(pattern), handler});
    }
    void Post(const std::string &pattern, const Handler &handler)
    {
        _post_route.push_back({std::regex(pattern), handler});
    }
    void Put(const std::string &pattern, const Handler &handler)
    {
        _put_route.push_back({std::regex(pattern), handler});
    }
    void Delete(const std::string &pattern, const Handler &handler)
    {
        _delete_route.push_back({std::regex(pattern), handler});
    }
    void SetThreadCount(int count)
    {
        _tcpserver.SetThreadCount(count);
    }
    void Listen()
    {
        _tcpserver.Start();
    }
};
