# ax_asr_api
C++ ASR API on Axera platforms

支持平台:  
 - AX650
 - AX630C
 - AX620Q

支持模型:
 - Whisper-Tiny
 - Whisper-Base
 - Whisper-Small
 - Whisper-Turbo
 - Sensevoice

## 更新

## 快速开始

使用示例:  
```cpp
#include "ax_asr_api.h"

AX_ASR_HANDLE handle = AX_ASR_Init(WHISPER_TINY, model_path);

char* result;
if (0 != AX_ASR_RunFile(handle, wav_file, language, &result)) {
    AX_ASR_Uninit(handle);
    return -1;
}

free(result);
AX_ASR_Uninit(handle);
```

## 下载模型

安装huggingface_hub
```bash
pip3 install -U huggingface_hub
```

运行下载脚本:
```bash
bash download_models.sh
```

## 编译

### 依赖

#### 系统要求

目前在Ubuntu 22.04上编译成功,  
需要安装CMake >= 3.13  

```bash
sudo apt install cmake build-essential
```

#### 获取交叉编译器

从[此处](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz)获取aarch64交叉编译器  
将其添加到PATH:
```bash
export PATH=$PATH:path of gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
```

### 获取BSP

```bash
bash download_bsp.sh
```

### 交叉编译

 - AX650
 ```bash
 bash build_ax650.sh
 ```
 编译完成后的产物在install/ax650下

 - AX630C
 ```bash
 bash build_ax630c.sh
 ```
  编译完成后的产物在install/ax630c下

 - AX620Q
 ```bash
 bash build_ax620q.sh
 ```
  编译完成后的产物在install/ax620q下

### 本地编译

暂不支持

### 其它编译选项

 - BUILD_TESTS  
 负责编译tests目录下的单元测试，可执行程序生成在install/ax650或install/ax630c下  
 ```bash
 bash build_ax650.sh -DBUILD_TESTS=ON
 ```

  - LOG_LEVEL_DEBUG  
  打印源码中的调试信息  
  ```bash
  bash build_ax650.sh -DLOG_LEVEL_DEBUG=ON
  ```    

## 测试

以下为tests下单元测试的使用示例和说明:

- test_whisper_tiny: 加载whisper tiny模型，打印demo.wav的识别结果
- test_whisper_base: 加载whisper base模型，打印demo.wav的识别结果
- test_whisper_small: 加载whisper small模型，打印demo.wav的识别结果
- test_whisper_turbo: 加载whisper turbo模型，打印demo.wav的识别结果
- test_sensevoice: 加载sensevoice模型，打印demo.wav的识别结果


## 性能表现

RTF(Real Time Factor)为推理时间除以音频时长，越小表示越快  
WER(Word Error Rate)为词错误率，在私有数据集上测试

 - RTF

 | asr_type | AX650 | AX630C |
| ------------- | ------ | ------ |
| Whisper-Tiny  | 0.0373 |        |
| Whisper-Base  | 0.0668 | 0.3849 |
| Whisper-Small | 0.2110 |        |
| Whisper-Turbo | 0.4372 |        |
| Sensevoice    | 0.0364 | 0.1170 |

 - WER

| asr_type      |  | 
| ------------- | ------ |
| Whisper-Tiny  |  0.24  |
| Whisper-Base  |  0.18  |
| Whisper-Small |  0.11  |
| Whisper-Turbo |  0.06  |
| Sensevoice    |  0.02  |

## 集成

编译产物包含 include/ax_asr_api.h 和 lib/libax_asr_api.so

## 讨论

 - Github issues
 - QQ 群: 139953715

## 贡献

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
