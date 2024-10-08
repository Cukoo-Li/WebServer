## ThreadPool

线程池用于实现高效地并发处理多个客户请求。

- 内部维护了一个任务队列和若干线程。
- 线程是任务队列的消费者，它们不断尝试从任务队列中取出任务，并执行之。为了避免忙等待，使用条件变量实现等待唤醒机制。
- `AddTask` 成员函数用于向任务队列中添加任务，供外部的生产者（在本项目中是主线程）调用。

## SqlConnPool

数据库连接池用于减少运行时动态申请和释放数据库连接所带来的开销。

- 内部维护了若干数据库连接
- `BorrowConn` 和 `ReturnConn` 成员函数分别用于借用和归还数据库连接，供用户（在本项目中是工作线程）调用。

## SqlConnGuard

使用 RAII 封装向数据库连接池借用和归还连接的操作，防止资源泄露。
