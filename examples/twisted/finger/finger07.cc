#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include <map>

using namespace muduo;
using namespace muduo::net;

typedef std::map<string, string> UserMap;
UserMap users;

string getUser(const string& user)
{
  string result = "No such user";
  UserMap::iterator it = users.find(user);
  if (it != users.end())
  {
    result = it->second;
  }
  return result;
}

void onMessage(const TcpConnectionPtr& conn,
               Buffer* buf,
               Timestamp receiveTime)
{
  const char* crlf = buf->findCRLF();
  if (crlf)
  {
    string user(buf->peek(), crlf);
    conn->send(getUser(user) + "\r\n");
    buf->retrieveUntil(crlf + 2);
    conn->shutdown();
  }
}

int main()
{
  users["schen"] = "Happy and well";//添加一个用户，不是空的user map
  EventLoop loop;
  TcpServer server(&loop, InetAddress(1079), "Finger");
  //注册消息可读时的回调函数：函数内遇到\r\n，查找用户，返回结果并断开连接
  server.setMessageCallback(onMessage);
  server.start();
  loop.loop();
}
