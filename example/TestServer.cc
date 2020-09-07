#include <Kenmuduo/TcpServer.h>
#include <Kenmuduo/Logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        :server_(loop_, addr, name), loop_(loop)
    {
        LOG_INFO("%s %s %d\n", __FILENAME__, __FUNCTION__, __LINE__);
        //注册回调函数
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, 
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        //设置合适的loop线程数量
        server_.setThreadNum(1);
        LOG_INFO("%s %s %d\n", __FILENAME__, __FUNCTION__, __LINE__);
    }
    void start()
    {
        LOG_INFO("%s %s %d\n", __FILENAME__, __FUNCTION__, __LINE__);
        server_.start();
        LOG_INFO("%s %s %d\n", __FILENAME__, __FUNCTION__, __LINE__);
    }
private:
    //连接建立或者断开回调函数
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            LOG_INFO("%s %s %d conn up:%s", __FILENAME__, __FUNCTION__, __LINE__, conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("%s %s %d conn down:%s", __FILENAME__, __FUNCTION__, __LINE__, conn->peerAddress().toIpPort().c_str());
        }
    }

    //可读写事件回调
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();
    }

    EventLoop* loop_;
    TcpServer server_;
};

int main()
{   
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    loop.loop();
    LOG_INFO("%s %s %d\n", __FILENAME__, __FUNCTION__, __LINE__);
    return 0;
}