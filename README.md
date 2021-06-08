## 功能

将.qsv格式的视频转码为.flv格式。



## 闲话

由于以前C#版本的代码过于丑陋，且存在部分文件转码失败的情况，该版本将被废弃，见[obsolete](https://github.com/btnkij/qsv2flv/tree/obsolete)分支。

当前分支下的代码将使用C重写，计划为控制台程序，暂时不打算提供UI界面。

该项目正在进行中，目前已具备基本功能，可转码大多数文件。但由于对qsv格式的部分细节存在未知，可能在转码部分文件时出错。



## 编译命令

```bash
gcc qsv2flv.c -o qsv2flv.exe -O2
```



## 使用方式

```bash
qsv2flv.exe input_file_name output_file_name
```



## 目录结构

* qsv2flv.c ：转码入口程序，负责提取flv数据，并使用yamdi注入metadata信息。
* yamdi.h ：拷贝自[ioppermann/yamdi: Yet Another MetaData Injector for FLV (github.com)](https://github.com/ioppermann/yamdi)，用于注入metadata信息。
* qsvformat.c ：用于格式说明的demo程序。

