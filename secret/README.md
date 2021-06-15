# qsvunpack
该路径下的代码不属于Qt工程文件，不具备实现转码的功能，仅用于原理说明。

对于以下内容，如果有大佬发现错误，或想要补充，或因版本更新失效，欢迎提出issue或pull request。



## 目录结构
**qsvformat.c** 用于格式说明的demo程序。

**qsvunpack.c** 用于提取分段视频并合并的demo程序，更健壮的算法见Qt工程中使用ffmpeg的实现。



## 格式分析

### 头部

| 偏移（绝对文件地址）    | 长度（字节）       | 字段名     | 解释                                      |
| ------------ | ------------------ | ---------- | ----------------------------------------- |
| 0x0          | 0xA                | signature | 标识符，"QIYI VIDEO"                      |
| 0xA          | 0x4                | version    | 版本号，0x01或0x02                        |
| 0xE          | 0x10               | vid        | 视频ID                                    |
| 0x1E         | 0x4                | _unknown1  | 未知，必须为0x01                          |
| 0x22         | 0x20               | _unknown2  | 未知，全部为0x00                          |
| 0x42         | 0x4                | _unknown3  | 未知，待分析                              |
| 0x46         | 0x4                | _unknown4  | 未知，待分析                              |
| 0x4A         | 0x8                | xml_offset | [xml字符串](#xml字符串)的绝对文件偏移     |
| 0x52         | 0x4                | xml_size   | [xml字符串](#xml字符串)的大小             |
| 0x56         | 0x4                | nb_indices | [索引](#索引)的数量                       |

### 索引

| 偏移（绝对文件地址） | 长度（字节） | 字段名         | 解释                   |
| ------------ | ------------------ | ---------- | ----------------------------------------- |
| 0x5A         | ceil(nb_index / 8) | _unknown_flag | 位标识，一个索引对应一位，待分析 |
| 上一字段之后 | nb_index * 0x1C    | indices    | 索引结构体数组                        |

每个索引结构体的大小为0x1C，经过加密，记录[视频分段](#视频分段)的偏移和大小。

| 偏移（相对于结构体首地址） | 长度（字节） | 字段名         | 解释                   |
| -------------------------- | ------------ | -------------- | ---------------------- |
| 0x0                        | 0x10         | _codetable     | 用于后面两个字段的解密 |
| 0x10                       | 0x8          | segment_offset | 视频分段的绝对文件偏移 |
| 0x18                       | 0x4          | segment_size   | 视频分段的大小         |

### xml字符串

加密的xml字符串和首尾的二进制数据，待分析。

### 视频分段

一个qsv文件包含多段视频文件，通过[索引](#索引)定位，前0x400字节经过加密。

已发现的视频格式有flv（旧版客户端）、mpeg-ts（新版客户端）。

每段视频有单独的头部和metadata数据，后一段的时间戳在前一段之后（不是从0开始计算）。