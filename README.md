## 简介

~~将.qsv格式的视频转码为.flv格式。~~



## 闲话

由于以前C#版本的代码过于丑陋，且存在部分文件转码失败的情况，该版本将被废弃，见[obsolete](https://github.com/btnkij/qsv2flv/tree/obsolete)分支。当前分支下的代码将使用C重写，计划为控制台程序，暂时不打算提供UI界面。

该项目正在进行中，目前仅可以转换使用旧客户端下载的视频。[qsvformat.c](https://github.com/btnkij/qsv2flv/blob/main/qsvformat.c)是一个演示转码原理的demo程序，可以提取视频分段的数据。从旧版本的文件中可以提取到flv格式的数据，简单合并即可完成转码。（新版本的文件中提取到的为ts格式的数据，因此简介被划掉了😑）



## 编译命令

```bash
gcc qsv2flv.c -o qsv2flv.exe -O2
```



## 使用方式

```bash
qsv2flv.exe input_file_name output_file_name
```



## 目录结构

* qsv2flv.c ：转码入口程序。
* yamdi.h ：拷贝自[ioppermann/yamdi: Yet Another MetaData Injector for FLV (github.com)](https://github.com/ioppermann/yamdi)，用于注入flv格式视频的metadata信息。
* qsvformat.c ：用于格式说明的demo程序。

