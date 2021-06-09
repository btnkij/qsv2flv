## 计划实现功能

~~将.qsv格式的视频转码为.flv格式。~~

由于qy客户端的更新，本项目的定位为：将qsv格式转换为常用视频格式，不限于flv。



## 闲话

由于以前C#版本的代码过于丑陋，且存在部分文件转码失败的情况，该版本将被废弃，见[obsolete](https://github.com/btnkij/qsv2flv/tree/obsolete)分支。当前分支下的代码将使用C++重写。

该项目正在进行中。

**qsvformat.c** ：已完成。用于格式说明的demo程序。

**qsvunpack.c** ：基本完成（可编译运行），目前能够提取qsv文件中的flv视频和ts视频，提取的视频能够播放。

**GUI** ：计划中，整合前面的算法，提供完整的转码流程。



## 使用方式

编译
```bash
gcc qsvunpack.c -o qsvunpack.exe -O2
```

运行（仅提取分段视频）
```bash
qsvunpack.exe input_file_name output_file_name
```

暂未提供完整的转码程序。
