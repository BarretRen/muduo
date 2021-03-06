// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Acceptor.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),//根据IP地址创建socket描述符
    acceptChannel_(loop, acceptSocket_.fd()),//创建对应的channel对象
    listening_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))//占位描述符，防止系统无描述符可用
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);//设置SO_REUSEADDR，可重入使用还未释放的IP和port
  acceptSocket_.setReusePort(reuseport);
  acceptSocket_.bindAddress(listenAddr);//bind socket IP
  acceptChannel_.setReadCallback(
      std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listening_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();//添加socket描述符到epoll wait list中一起监听
}

void Acceptor::handleRead()//可读事件回调，说明有新client连接
{
  loop_->assertInLoopThread();
  InetAddress peerAddr;
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr);//调用新连接回调，传递connection id
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE)
    {
      ::close(idleFd_);//accept失败，说明系统没有描述符可用，需要先释放占位用的描述符
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);//占位描述符现在标识的时connection id，关闭它
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);//重新占位
    }
  }
}

