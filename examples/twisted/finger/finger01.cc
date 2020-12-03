#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

int main()
{
  //什么都不做，程序空等
  EventLoop loop;
  loop.loop();
}
