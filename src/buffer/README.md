## Buffer

- 内部维护了一个 `char` 数组（`vector<char>`），可以用于存储请求报文或响应报文数据。
- 数组中的字节数据从左到右可以分为三个区间：
  - `[0, read_pos_)` - 为已经读取的区间（这部分数据已经没有必要再保留，后续可以回收其空间）
  - `[read_pos_, write_pos_)` - 为待读取的区间
  - `[write_pos_, end)` - 待写入的区间，没有数据
  > 这三个区间中的字节数量分别可以通过 `PrependableBytes`、`ReadableBytes` 和 `WritableBytes` 获取。
- `ReadFd` 和 `WriteFd` 成员函数允许我们从`fd` 和 `Buffer` 之间的进行数据转移。
  - `ReadFd` - 读取 `fd` 中的数据，写入到 `Buffer` 中
  - `WriteFd` - 读取 `Buffer` 中的数据，写入到 `fd` 中
- `Append` 成员函数允许我们向缓冲区中添加数据。
- 也可以通过 `ReadBegin()` 成员函数获取读指针，进而直接获取待读取的数据，之后务必调用 `Retrieve` 系列函数更新读指针。
