# qsv2flv
将iqy的qsv视频格式无损转换为mp4、flv等通用格式的小工具。
（不要在意标题，很久以前起的了）

下载地址：测试无误后再放上来

## 对普通用户说的话
1. 推荐转码为mp4，部分视频转码为flv会失败。
**科普**：某些高清片源使用了.h265视频编码格式，不能被无损地封装为flv格式。使用客户端下载的视频中存在错误的帧（正常情况，不影响播放），也不能被封装为flv格式。mp4是网络上应用最广格式，几乎所有播放器都支持，能够适应绝大多数需求。
2. 遇到转码失败，如果是中文错误提示，很大概率是视频本身的问题。遇到英文错误提示，一定是工具本身的问题。
3. 由于作者较菜，~~界面很符合程序员的审美，~~工具可能存在bug。如果使用中遇到问题，欢迎提出issue，在不涉及隐私的情况下，可以提供转码失败视频的下载地址，或客户端版本+视频名称+清晰度。

## 对开发者说的话
作者没有流媒体处理的基础、没有使用ffmpeg的经验，工具可能存在隐藏的bug，甚至有内存泄露。如果你~~忍无可忍~~想要自己修改，可以参考本节。

### 作者的开发环境
Qt 6.1.1
MSVC 2019 64-bit 编译器
ffmpeg 4.4 64-bit

ffmpeg的头文件放在ffmpeg/include下，静态库放在ffmpeg/lib下。运行时请将动态库拷贝到编译路径下。

### 目录结构
**ffmpeg/** 存放ffmpeg的库文件。

**secret/** 不属于Qt的工程文件。如果想了解转码原理，请[移步于此](https://github.com/btnkij/qsv2flv/tree/main/secret)。

**main.cpp** 程序入口，由Qt生成，不需要修改。

**mainwindow** 处理与前端的交互。

**datamodel、inputfilemodel** 全局的数据模型，保存前端的输入，供其他模块读取。

**qsvreader** 核心算法，负责提取qsv中分段的流数据。

**converter** 核心算法，负责合并分段视频，封装为目标格式。

### 待解决的问题
1. 处理没有设置时间戳的packet
```
Timestamps are unset in a packet for stream 0. This is deprecated and will stop working in the future. Fix your code to set the timestamps properly
Application provided invalid, non monotonically increasing dts to muxer in stream 0: 806250 >= NOPTS
```
目前使用的解决办法：为输出上下文（AVFormatContext）设置AVFMT_NOTIMESTAMPS标记。
```c++
// converter.cpp
// AVFormatContext* createOutputContext(const char* outputPath, AVFormatContext* inCtx);
outCtx->oformat->flags |= AVFMT_NOTIMESTAMPS;
```
2. 大视频的seek速度较慢，可能没有添加索引表。