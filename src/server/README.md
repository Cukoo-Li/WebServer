## Epoller

Epoller 类封装了对 epoll 内核事件表的增、删、改等操作，提供更便捷的函数接口，避免后续需要直接调用繁琐的 `epoll_wait` 和 `epoll_ctl` 函数。

## WebServer

WebServer 类包含 Web 服务器的基本信息以及运行时需要的资源。

构造函数负责对运行时需要的一系列资源进行初始化操作，这些资源主要包括监听 socket、epoll 内核事件表、线程池、数据库连接池、定时器容器。

`Startup` 是 WebServer 类唯一的公有成员函数，描述了 Web 服务器整个生命周期所做的（两件）事情：

- 监听多个文件描述符上的 I/O 事件（通过 epoll IO 复用）
  
  - 监听 socket 上的可读事件 - 调用 `accept` 接收新的客户连接，并为其设置定时器、注册事件
  - 连接 socket 上的可读事件 - 让工作线程去读取（`OnRead`）并处理（`OnProcess`）数据
  - 连接 socket 上的可写事件 - 让工作线程去写入数据（`OnWrite`）
  
- 处理定时事件（通过 `epoll_wait` 的超时参数实现定时）
