#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpServer.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;
//消息格式为：两个int64，index 0保存client时间，index 1保存server时间
const size_t frameLen = 2*sizeof(int64_t);

void serverConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    //广域网中App记录的发包时间与内核真正发出数据包的时间之差不再是一个可以忽略的小间隔
    //所以需要设置弄Delay
    conn->setTcpNoDelay(true);
  }
  else
  {
  }
}

void serverMessageCallback(const TcpConnectionPtr& conn,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)//收到的数据达到指定长度
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);
    message[1] = receiveTime.microSecondsSinceEpoch();//填写server端当前时间
    conn->send(message, sizeof message);
  }
}

void runServer(uint16_t port)
{
  EventLoop loop;
  TcpServer server(&loop, InetAddress(port), "ClockServer");
  server.setConnectionCallback(serverConnectionCallback);
  server.setMessageCallback(serverMessageCallback);
  server.start();
  loop.loop();
}

TcpConnectionPtr clientConnection;

void clientConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    clientConnection = conn;
    conn->setTcpNoDelay(true);
  }
  else
  {
    clientConnection.reset();
  }
}

void clientMessageCallback(const TcpConnectionPtr&,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);
    int64_t send = message[0];//取出本端之前的发送时间
    int64_t their = message[1];//取出server端收到的时间
    int64_t back = receiveTime.microSecondsSinceEpoch();//获取当前client时间
    int64_t mine = (back+send)/2;
    LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;
  }
}

void sendMyTime()
{
  if (clientConnection)
  {
    int64_t message[2] = { 0, 0 };
    message[0] = Timestamp::now().microSecondsSinceEpoch();
    clientConnection->send(message, sizeof message);
  }
}

void runClient(const char* ip, uint16_t port)
{
  EventLoop loop;
  TcpClient client(&loop, InetAddress(ip, port), "ClockClient");
  client.enableRetry();
  client.setConnectionCallback(clientConnectionCallback);
  client.setMessageCallback(clientMessageCallback);
  client.connect();
  loop.runEvery(0.2, sendMyTime);//每0.2秒发送一次
  loop.loop();
}

int main(int argc, char* argv[])
{
  if (argc > 2)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    if (strcmp(argv[1], "-s") == 0)
    {
      runServer(port);
    }
    else
    {
      runClient(argv[1], port);
    }
  }
  else
  {
    printf("Usage:\n%s -s port\n%s ip port\n", argv[0], argv[0]);
  }
}

