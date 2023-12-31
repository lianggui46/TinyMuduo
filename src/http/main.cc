#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpContext.h"
#include "Timestamp.h"
#include "AsyncLogging.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/uio.h>
#include <fcntl.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

bool benchmark = true;

void cb(const HttpRequest& req, HttpResponse* resp)
{
    //std::cout << "Headers " << req.methodString() << " " << req.path() << std::endl;
    
    // 打印头部
    if (!benchmark)
    {
        const std::unordered_map<std::string, std::string>& headers = req.headers();
        for (const auto& header : headers)
        {
            std::cout << header.first << ": " << header.second << std::endl;
        }
        std::cout << std::endl;
    }

    if (req.path() == "/")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->addHeader("Server", "Muduo");
        std::string now = Timestamp::now().toFormattedString();
        resp->setBody("<html><head><title>This is title</title></head>"
            "<body><h1>Hello</h1>Now is " + now +
            "</body></html>");
    }
    else if (req.path() == "/hello")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->addHeader("Server", "Muduo");
        resp->setBody("hello, world!\n");
    }
    else{
        std::string doc_root = "/home/lianggui/tinymuduo/src/http/resources/";
        std::string filename =  doc_root + req.path();
        struct stat m_file_stat;

        // 获取文件的相关的状态信息，-1失败，0成功
        if (unlikely( ::stat(filename.c_str(), &m_file_stat ) < 0 )) {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
            return;
        }
        //查询文件的读权限
        if(! ( m_file_stat.st_mode & S_IROTH ) ){
            resp->setStatusCode(HttpResponse::k403Forbidden);
            resp->setStatusMessage("Forbidden");
            resp->setCloseConnection(true);
            return;
        }

        // 以只读方式打开文件
        int fd = open(filename.c_str(), O_RDONLY );
        if(unlikely(fd < 0)){
            resp->setStatusCode(HttpResponse::k400BadRequest);
            resp->setStatusMessage("Bad Request");
            resp->setCloseConnection(true);
            return;
        }

        // 创建内存映射
        void* m_file_address = ::mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
        
        if(filename.c_str() == "/home/lianggui/tinymuduo/src/http/resources/images/image1.jpg"){
            std::cout << m_file_address << std::endl;
            std::cout << m_file_stat.st_size << std::endl;
        }

        if(unlikely(m_file_address == MAP_FAILED)){
            resp->setStatusCode(HttpResponse::k400BadRequest);
            resp->setStatusMessage("Bad Request");
            resp->setCloseConnection(true);
            LOG_ERROR << "MAP_FAILED";
            return;
        }
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");

        const std::string & rp = req.path();
        int idx = rp.find_last_of('.');

        if(idx == std::string::npos){
            resp->setContentType("text/plain"); 
        }

        else{
            std::string fmt = rp.substr(idx);
            if(fmt == ".html"){
                resp->setContentType("text/html"); 
            }
            else if(fmt == ".css"){
                resp->setContentType("text/css");
            }
            else if(fmt == ".png"){
                resp->setContentType("image/png");
            }
            else if(fmt == ".jpg"){
                resp->setContentType("image/jpeg");
            }
            else if(fmt == ".bmp"){
                resp->setContentType("image/bmp");
            }
            else if(fmt == ".gif"){
                resp->setContentType("image/gif");
            }
            else if(fmt == ".gif"){
                resp->setContentType("image/gif");
            }
            else if(fmt == ".mp4"){
                resp->setContentType("video/mp4");
            }
            else{
                resp->setContentType("text/plain");
            }
        }
        
        resp->addHeader("Server", "Muduo");
        resp->setBody(std::string(static_cast<char *>(m_file_address), m_file_stat.st_size));

        ::close(fd);
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = NULL;
    }
}

int kRollSize = 500*1000*1000;

// 异步日志
std::unique_ptr<AsyncLogging> g_asyncLog;

void asyncOutput(const char* msg, int len)
{
    g_asyncLog->append(msg, len);
}

void setLogging(const char* argv0)
{
    Logger::setOutput(asyncOutput);
    char name[256];
    strncpy(name, argv0, 256);
    g_asyncLog.reset(new AsyncLogging(::basename(name), kRollSize));
    g_asyncLog->start();
}

int main(int argc, char* argv[])
{
    setLogging("LOG");
    EventLoop loop;
    HttpServer server(&loop, InetAddress(80), "http-server");
    server.setHttpCallback(cb);
    server.start();
    loop.loop();
}
