# ax_asr_api
C++ ASR API on Axera platforms

支持平台:  
 - AX650
 - AX630C
 - AX620Q
 - AX8850

支持模型:
 - Whisper-Tiny
 - Whisper-Base
 - Whisper-Small
 - Whisper-Turbo
 - Sensevoice

## 文档目录  
- [快速开始](#快速开始)  
- [下载模型](#下载模型)  
- [编译](#编译)  
- [HTTP API](#http-api)
- [OpenAPI 描述](#openapi-描述)
- [Python Binding](#python-binding)
- [C-SDK](#c-sdk)
- [测试](#测试)  
- [性能表现](#性能表现)  
- [集成](#集成)  
- [讨论](#讨论)  

## 更新

## 快速开始

可从Release页面下载预编译库  

使用示例:  
```cpp
#include "ax_asr_api.h"

AX_ASR_HANDLE handle = AX_ASR_Init(AX_WHISPER_TINY, "./models-ax650");
if (!handle) {
    return -1;
}

char* result = NULL;
int ret = AX_ASR_RunFile(handle, "demo.wav", "zh", &result);
if (ret != AX_ASR_SUCCESS) {
    AX_ASR_Uninit(handle);
    return -1;
}

AX_ASR_Free(result);
AX_ASR_Uninit(handle);
```

说明:

- `model_path` 传模型根目录，例如 `./models-ax650`
- `AX_ASR_RunFile` 支持 `wav` 和 `mp3`
- 返回字符串必须使用 `AX_ASR_Free` 释放
- 同一个 `AX_ASR_HANDLE` 不建议被多个线程并发调用

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

 - AX650/AX630C(aarch64)
从[此处](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz)获取aarch64交叉编译器  
将其添加到PATH:
```bash
export PATH=$PATH:path of gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
```

 - AX620Q(arm-uclibc-linux)
从[此处](https://github.com/AXERA-TECH/ax620q_bsp_sdk/releases/download/v2.0.0/arm-AX620E-linux-uclibcgnueabihf_V3_20240320.tgz)获取
```bash
export PATH=$PATH:path of arm-AX620E-linux-uclibcgnueabihf/bin
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

 - AX8850
 ```bash
 bash build_ax8850_aarch64.sh
 ```
  编译完成后的产物在install/ax8850_aarch64下

### 本地编译

暂不支持

### 其它编译选项

 - BUILD_TESTS 默认OFF  
 负责编译tests目录下的单元测试，可执行程序生成在install/ax650或install/ax630c下  
 ```bash
 bash build_ax650.sh -DBUILD_TESTS=ON
 ```

  - LOG_LEVEL_DEBUG 默认OFF  
  打印源码中的调试信息  
  ```bash
  bash build_ax650.sh -DLOG_LEVEL_DEBUG=ON
  ```    

  - BUILD_SERVER 默认ON   
  编译asr_server  
  ```bash
  bash build_ax650.sh -DBUILD_SERVER=ON
  ```    

## HTTP API（OpenAI 兼容）

服务端默认提供以下接口:


- `POST /v1/audio/transcriptions`
- `GET /v1/models`
- `GET /healthz`


可直接使用 OpenAI 官方 Python SDK 调用：

```python
from openai import OpenAI

client = OpenAI(base_url="http://<ip>:8080/v1", api_key="your_token")
with open("demo.wav", "rb") as f:
    transcript = client.audio.transcriptions.create(
        model="sensevoice", file=f, language="zh")
print(transcript.text)
```

### 认证

当 `asr_server` 启动时传入 `--api_key your_token`，客户端需要携带:

```http
Authorization: Bearer your_token
```

如果不传 `--api_key`，则不启用鉴权。

### 模型名

原生模型名:

- `sensevoice`
- `whisper_tiny`
- `whisper_base`
- `whisper_small`
- `whisper_turbo`

兼容别名:

- `gpt-4o-transcribe` -> `sensevoice`
- `gpt-4o-mini-transcribe` -> `sensevoice`
- `whisper-1` -> `whisper_turbo`

### POST /v1/audio/transcriptions

请求类型: `multipart/form-data`

请求字段:

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `file` | 是 | 音频文件，当前支持 `.wav` 和 `.mp3` |
| `model` | 是 | 见上面的模型名 |
| `language` | 否 | `sensevoice` 默认 `auto`，`whisper_*` 默认 `en` |
| `response_format` | 否 | `json`、`text`、`verbose_json`，默认 `json` |

语言说明:

- `sensevoice`: `auto`、`zh`、`en`、`yue`、`ja`、`ko`
- `whisper_*`: 语言列表由对应模型配置文件决定，服务端会校验非法值

`response_format` 返回约定:

- `json`: `{"text":"..."}`
- `text`: 纯文本响应
- `verbose_json`: 当前返回 `text`、`model`、`language`、`request_id`

注意:

- 当前 `verbose_json` 还不包含 `segments`、时间戳或说话人信息
- 当前接口定位是“单文件同步转写”

#### cURL 示例

```bash
curl http://127.0.0.1:8080/v1/audio/transcriptions \
  -H "Authorization: Bearer your_token" \
  -F file="@demo.wav" \
  -F model="sensevoice" \
  -F language="zh" \
  -F response_format="json"
```

成功响应:

```json
{
  "text": "甚至出现交易几乎停滞的情况"
}
```

`verbose_json` 响应示例:

```json
{
  "language": "auto",
  "model": "sensevoice",
  "request_id": "axasr-1780307147723-0",
  "text": "甚至出现交易几乎停滞的情况"
}
```

错误响应示例:

```json
{
  "error": {
    "message": "Unsupported language \"invalid_lang\" for model whisper_tiny.",
    "type": "invalid_request_error",
    "param": "language",
    "code": 400
  }
}
```

常见状态码:

| 状态码 | 含义 |
| --- | --- |
| `200` | 成功 |
| `400` | 参数错误、文件格式错误、语言不支持 |
| `401` | 缺少或错误的 Bearer Token |
| `404` | 路径不存在或模型不可用 |
| `413` | 上传文件超过服务端限制 |
| `500` | 服务端内部错误 |

### GET /v1/models

用于查询当前服务暴露的模型列表。

示例:

```bash
curl http://127.0.0.1:8080/v1/models \
  -H "Authorization: Bearer your_token"
```

### GET /healthz

用于健康检查，不要求鉴权。

示例响应:

```json
{
  "auth_enabled": true,
  "status": "ok"
}
```

## OpenAPI 描述

仓库提供了更正式的 OpenAPI 3 描述文件:

- [docs/openapi.yaml](/opt/rzyang/Github/ax_asr_api/docs/openapi.yaml)

说明:

- 该文件描述的是当前仓库已实现能力，不包含尚未支持的流式识别、时间戳、分段或说话人能力
- `verbose_json` 在当前实现中只返回 `text`、`model`、`language`、`request_id`

如果本地安装了 Swagger UI、Redoc 或其它 OpenAPI 工具，可以直接加载 `docs/openapi.yaml` 查看。

## C-SDK

头文件: `include/ax_asr_api.h`  
动态库: `lib/libax_asr_api.so`

### 核心接口

```c
AX_ASR_HANDLE AX_ASR_Init(AX_ASR_TYPE_E asr_type, const char* model_path);
void AX_ASR_Uninit(AX_ASR_HANDLE handle);
int AX_ASR_RunFile(AX_ASR_HANDLE handle, const char* wav_file, const char* language, char** result);
int AX_ASR_RunPCM(AX_ASR_HANDLE handle, float* pcm_data, int num_samples, int sample_rate, const char* language, char** result);
void AX_ASR_Free(char* result);
```

### 返回码

| 返回码 | 含义 |
| --- | --- |
| `AX_ASR_SUCCESS` | 成功 |
| `AX_ASR_ERR_INVALID_ARGUMENT` | 参数非法 |
| `AX_ASR_ERR_INIT_FAILED` | 初始化失败 |
| `AX_ASR_ERR_AUDIO_LOAD_FAILED` | 音频加载失败 |
| `AX_ASR_ERR_RUN_FAILED` | 推理失败 |
| `AX_ASR_ERR_NO_MEMORY` | 内存分配失败 |

### 使用约束

- `model_path` 传模型根目录，不是子目录
- `AX_ASR_RunFile` 读取文件路径；`AX_ASR_RunPCM` 适合上层自行管理音频流
- `AX_ASR_RunPCM` 的输入为单声道 `float` PCM，范围 `-1.0 ~ 1.0`
- 返回文本由库内分配，调用方必须使用 `AX_ASR_Free`

## Python Binding

通过 pybind11 封装 `libax_asr_api.so`，提供 Pythonic 的 `AX_ASR` 类。

### 构建

```bash
cd python
pip install . --config-settings=cmake.define.AX_ASR_LIB_DIR=<install_dir>/lib
```

需要 `pybind11>=3.0` 和 CMake >= 3.13。

### 快速开始

```python
from ax_asr import AX_ASR

# 使用 context manager 自动管理生命周期
with AX_ASR("sensevoice", "./models-ax650") as asr:
    text = asr.transcribe_file("demo.wav", language="zh")
    print(text)
```

### 核心接口

```python
class AX_ASR:
    def __init__(self, model_type: str, model_path: Optional[str] = None):
        """
        model_type: whisper_tiny, whisper_base, whisper_small, whisper_turbo, sensevoice
        model_path: 模型根目录，默认读取 AX_ASR_MODEL_PATH 环境变量
        """

    def transcribe_file(self, audio_path: str, language: str = "zh") -> str:
        """转写音频文件，支持 .wav 和 .mp3"""

    def transcribe_pcm(self, pcm: np.ndarray, sample_rate: int, language: str = "zh") -> str:
        """转写 PCM float32 单声道音频 (numpy.ndarray, shape=(N,), range [-1.0, 1.0])"""

    def close(self) -> None:
        """释放 ASR handle，可多次调用"""
```

### 使用约束

- `model_path` 传模型根目录，与 C-SDK 一致
- `transcribe_file` 内部已处理音频加载和重采样
- `transcribe_pcm` 接受 `(N,)` 形状的 float32 numpy 数组
- C 层错误码自动转换为 Python `RuntimeError`
- 同一个 `AX_ASR` 实例不建议并发调用；多并发请创建多个实例
- 同一个 `AX_ASR_HANDLE` 不建议并发调用；如果需要多并发，请创建多个 handle

## 测试

### 主程序

```bash
./install/ax650/main -a demo.wav -t whisper_tiny -p ./models-ax650 -l zh
```

Usage:

```text
./install/ax8850_aarch64/main --help
usage: ./install/ax8850_aarch64/main --audio=string --model_type=string [options] ...
options:
  -a, --audio         audio file, support wav and mp3 (string)
  -t, --model_type    Choose from whisper_tiny, whisper_base, whisper_small, whisper_turbo, sensevoice (string)
  -p, --model_path    model path which contains axmodel (string [=./models-ax650])
  -l, --language      en, zh (string [=zh])
  -?, --help          print this message
```

### 服务端(asr_server)

```bash
./install/ax8850_aarch64/asr_server --port 8080 --model_path ./models-ax650 --api_key your_token
```

Usage:

```text
./install/ax8850_aarch64/asr_server --help
usage: ./install/ax8850_aarch64/asr_server [options] ...
options:
  -p, --port          On which port to run the server (int [=8080])
  -m, --model_path    model path which contains axmodel (string [=./models-ax650])
  -k, --api_key       Bearer token required by the server. Empty means auth disabled. (string [=])
      --payload_limit_mb  Maximum accepted upload size in MiB (int [=25])
      --read_timeout_sec  Socket read timeout in seconds (int [=120])
      --write_timeout_sec Socket write timeout in seconds (int [=120])
  -?, --help          print this message
```

### Python 客户端

```bash
cd scripts
pip install openai
python test_asr_server.py --ip 10.126.33.146 --port 8080 --audio ../demo.wav -m sensevoice -l zh --api-key your_token
```

更多参数见:

```bash
python test_asr_server.py --help
```

### 单元测试

以下为 `tests/` 下单元测试的使用示例和说明:

- `test_whisper_tiny`: 加载 whisper tiny 模型，打印 `demo.wav` 的识别结果
- `test_whisper_base`: 加载 whisper base 模型，打印 `demo.wav` 的识别结果
- `test_whisper_small`: 加载 whisper small 模型，打印 `demo.wav` 的识别结果
- `test_whisper_turbo`: 加载 whisper turbo 模型，打印 `demo.wav` 的识别结果
- `test_sensevoice`: 加载 sensevoice 模型，打印 `demo.wav` 的识别结果


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

Python 用户可直接安装 wheel 包：
```bash
pip install ax_asr-*.whl
```

## 讨论

 - Github issues
 - QQ 群: 139953715

## 贡献

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
