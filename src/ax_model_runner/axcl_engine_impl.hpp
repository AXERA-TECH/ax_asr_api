/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#if defined (CHIP_AX8850)

#pragma once

#include <vector>
#include <string>
#include <string.h>

#include "ax_model_runner.hpp"
#include "axcl.h"
#include "axcl_engine_guard.hpp"
#include "utils/memory_utils.hpp"
#include "utils/logger.h"

static bool set_device(int32_t device_index) {
    axclrtDeviceList lst;
    auto ret = axclrtGetDeviceList(&lst);
    if (ret != 0) {
        ALOGE("axclrtGetDeviceList failed! ret=0x%x", ret);
        return false;
    }

    if (lst.num == 0) {
        ALOGE("Found 0 device.");
        return false;
    }

    // device_id counts from 0
    if (device_index < 0 || device_index >= lst.num) {
        ALOGE("Invalid device_index: %d. Valid range: 0-%d",
                        device_index, lst.num - 1);
        return false;
    }

    int32_t device_id_rt = lst.devices[device_index];

    ret = axclrtSetDevice(device_id_rt);
    if (ret != 0) {
        ALOGE("Failed to call axclrtSetDevice(). Return code is: %d",
                        static_cast<int32_t>(ret));
        return false;
    }

    return true;
}  

class AxModelRunner::Impl {
public:
    Impl() { }

    ~Impl() {
        unload_model();
    }

    /*
    Initialization step:

    1. AxclInit()
    2. set device
    3. init engine
    4. axclrtEngineLoadFromMem or axclrtEngineLoadFromFile
    5. axclrtEngineCreateContext
    */
    int load_model(const char* model_path, AX_IO_BUFFER_STRATEGY_T strategy, int device_index) {
        if (!utils::file_exist(std::string(model_path))) {
            ALOGE("model path %s not exist!", model_path);
            return -1;
        }

        m_engine_guard = std::make_unique<AxclEngineGuard>(nullptr, AXCL_VNPU_DISABLE, device_index, set_device);

        // if (!set_device_(device_index)) {
        //     ALOGE("set device %d failed!", device_index);
        //     return -1;
        // }

        auto ret = axclrtEngineLoadFromFile(model_path, &model_id_);
        if (ret != 0) {
            ALOGE("axclrtEngineLoadFromFile failed! ret=0x%x", ret);
            model_id_ = 0;
            return -1;
        }

        ret = axclrtEngineCreateContext(model_id_, &context_id_);
        if (ret != 0) {
            ALOGE("axclrtEngineLoadFromFile failed! ret=0x%x", ret);
            context_id_ = 0;
            model_id_ = 0;
            return -1;
        }

        if (!prepare_io_(strategy, this->group_, this->batch_)) {
            ALOGE("prepare_io_ failed! ret=0x%x", ret);
            this->unload_model();
            return -1;
        }

        m_strategy = strategy;
        m_loaded = true;

        return 0;
    }

    int unload_model(void) {
        int ret = 0;
        if (m_loaded) {
            if (0 != this->model_id_) {
                for (auto& input : inputs_) {
                    if (nullptr != input) {
                        std::ignore = axclrtFree(input);
                        input = nullptr;
                    }
                }
                for (auto& output : outputs_) {
                    if (nullptr != output) {
                        std::ignore = axclrtFree(output);
                        output = nullptr;
                    }
                }

                ret = axclrtEngineDestroyIOInfo(this->info_);
                if (ret != 0) {
                    ALOGE("axclrtEngineDestroyIOInfo failed! ret=0x%x", ret);
                    return ret;
                }

                ret = axclrtEngineDestroyIO(this->io_);
                if (ret != 0) {
                    ALOGE("axclrtEngineDestroyIO failed! ret=0x%x", ret);
                    return ret;
                }

                ret = axclrtEngineUnload(this->model_id_);
                if (ret != 0) {
                    ALOGE("axclrtEngineDestroyIO failed! ret=0x%x", ret);
                    return ret;
                }
            }

            m_loaded = false;
        }
        return ret;
    }

    int run(void) {
        if (!m_loaded) {
            ALOGE("Model is not loaded! Call load_model first");
            return -1;
        }

        if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED) {
            for (int index = 0; index < m_input_num; index++) {
                
            }
        }

        if (const auto ret = axclrtEngineExecute(this->model_id_, this->context_id_, this->group_, this->io_); 0 != ret) {
            ALOGE("Run model failed{0x%08X}.\n", ret);
            return ret;
        }

        return 0;
    }

    int set_input(int index, void* data) {
        if (index < 0)  index += m_input_num;
        if (index > m_input_num - 1) {
            ALOGE("index(%d) exceed input_num(%d)", index, m_input_num);
            return -1;
        }

        if (!data) {
            ALOGE("data is null");
            return -1;
        }

        axclrtMemcpy(inputs_[index], data, inputs_size_[index], AXCL_MEMCPY_DEVICE_TO_DEVICE);

        return 0;
    }

    int set_inputs(const std::vector<void*>& datas) {
        for (int index = 0; index < m_input_num; index++) {
            void* data = datas[index];
            if (!data) {
                ALOGE("index %d data is null", index);
                return -1;
            }

            if (0 != set_input(index, data)) {
                ALOGE("set_input of index %d failed!", index);
                return -1;
            }
        }

        return 0;
    }

    int get_output(int index, void* data) {
        if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED)
            axclrtMemFlush(outputs_[index], outputs_size_[index]);

        axclrtMemcpy(data, outputs_[index], outputs_size_[index], AXCL_MEMCPY_DEVICE_TO_DEVICE);

        return 0;
    }

    int get_outputs(const std::vector<void*>& datas) {
        for (int index = 0; index < m_output_num; index++) {
            void* data = datas[index];
            if (!data) {
                ALOGE("index %d data is null", index);
                return -1;
            }

            if (0 != get_output(index, data)) {
                ALOGE("get_output of index %d failed!", index);
                return -1;
            }
        }
        
        return 0;
    }

    inline int get_input_num(void) {
        return m_input_num;
    }

    inline int get_output_num(void) {
        return m_output_num;
    }

    inline void* get_input_ptr(int index) {
        return inputs_[index];
    }

    void* get_output_ptr(int index) {
        if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED)
            axclrtMemFlush(outputs_[index], outputs_size_[index]);

        return outputs_[index];
    }

    inline uint64_t get_input_phy_addr(int index) {
        return 0;
    }

    inline uint64_t get_output_phy_addr(int index) {
        return 0;
    }

    inline const char* get_input_name(int index) {
        return axclrtEngineGetInputNameByIndex(this->info_, index);
    }

    inline const char* get_output_name(int index) {
        return axclrtEngineGetOutputNameByIndex(this->info_, index);
    }

    inline int get_input_size(int index) {
        return inputs_size_[index];
    }

    inline int get_output_size(int index) {
        return outputs_size_[index];
    }

    std::vector<int> get_input_shape(int index) {
        return input_tensor_shapes_[index];
    }

    std::vector<int> get_output_shape(int index) {
        return output_tensor_shapes_[index];
    }

private:
    bool prepare_io_(AX_IO_BUFFER_STRATEGY_T strategy, const uint32_t& group, const uint32_t& batch) {
        // 0. check the handle
        if (0 == this->model_id_) {
            ALOGE("Model id is not set, load model first.");
            return false;
        }

        // 1. get the IO info
        auto ret = axclrtEngineGetIOInfo(this->model_id_, &this->info_);
        if (0 != ret) {
            ALOGE("Get model io info failed{0x%08X}.", ret);
            return false;
        }

        // 2. get the count of shape group
        int32_t total_group = 0;
        ret = axclrtEngineGetShapeGroupsCount(this->info_, &total_group);
        if (0 != ret) {
            ALOGE("Get model shape group count failed{0x%08X}.", ret);
            return false;
        }

        // 3. check the group index
        if (group >= static_cast<decltype(group)>(total_group)) {
            ALOGE("Model group{%d} is out of range{total %d}.", group, total_group);
            return false;
        }
        this->group_ = static_cast<int32_t>(group);

        // 4. check the batch size
        this->batch_ = (0 == batch ? 1 : batch);

        // 5. get the count of inputs
        uint32_t input_count = 0;
        if (input_count = axclrtEngineGetNumInputs(this->info_); 0 == input_count) {
            ALOGE("Get model input count failed.");
            return false;
        }

        // 6. get the count of outputs
        uint32_t output_count = 0;
        if (output_count = axclrtEngineGetNumOutputs(this->info_); 0 == output_count) {
            ALOGE("Get model output count failed.");
            return false;
        }

        // 7. prepare the input and output
        m_input_num = input_count;
        m_output_num = input_count;
        this->inputs_.resize(input_count, nullptr);
        this->inputs_size_.resize(input_count, 0);
        this->outputs_.resize(output_count, nullptr);
        this->outputs_size_.resize(output_count, 0);

        // 8. prepare the memory, inputs
        for (uint32_t i = 0; i < input_count; i++) {
            uint32_t original_size = 0;
            if (original_size = axclrtEngineGetInputSizeByIndex(this->info_, group, i); 0 == original_size) {
                ALOGE("Get model input{index: %d} size failed.\n", i);
                return false;
            }

            this->inputs_size_[i] = original_size * this->batch_;

            if (strategy == AX_IO_BUFFER_STRATEGY_CACHED) {
                if (const auto ret = axclrtMallocCached(&this->inputs_[i], this->inputs_size_[i], axclrtMemMallocPolicy{}); 0 != ret) {
                    ALOGE("Memory allocation for tensor{index: %d} failed{0x%08X}.", i, ret);
                    return false;
                }
            } else {
                if (const auto ret = axclrtMalloc(&this->inputs_[i], this->inputs_size_[i], axclrtMemMallocPolicy{}); 0 != ret) {
                    ALOGE("Memory allocation for tensor{index: %d} failed{0x%08X}.", i, ret);
                    return false;
                }
            }

            axclrtEngineIODims input_dims;
            if (const auto ret = axclrtEngineGetInputDims(this->info_, this->group_, i, &input_dims); 0 != ret) {
                ALOGE("axclrtEngineGetInputDims of input %d failed! ret=0x%08X", i, ret);
                return false;
            }
            this->input_tensor_shapes_.emplace_back(std::vector<int>(input_dims.dims, input_dims.dims + input_dims.dimCount));
            // clean memory, some cases model may need to clean memory
            // axclrtMemset(this->inputs_[i], 0, size);
        }

        // 9. prepare the memory, outputs
        for (uint32_t i = 0; i < output_count; i++) {
            uint32_t original_size = 0;
            if (original_size = axclrtEngineGetOutputSizeByIndex(this->info_, group, i); 0 == original_size) {
                ALOGE("Get model output{index: %d} size failed.", i);
                return false;
            }

            this->outputs_size_[i] = original_size * this->batch_;

            if (strategy == AX_IO_BUFFER_STRATEGY_CACHED) {
                if (const auto ret = axclrtMallocCached(&this->outputs_[i], this->outputs_size_[i], axclrtMemMallocPolicy{}); 0 != ret) {
                    ALOGE("Memory allocation for tensor{index: %d} failed{0x%08X}.", i, ret);
                    return false;
                }
            } else {
                if (const auto ret = axclrtMalloc(&this->outputs_[i], this->outputs_size_[i], axclrtMemMallocPolicy{}); 0 != ret) {
                    ALOGE("Memory allocation for tensor{index: %d} failed{0x%08X}.", i, ret);
                    return false;
                }
            }

            axclrtEngineIODims output_dims;
            if (const auto ret = axclrtEngineGetOutputDims(this->info_, this->group_, i, &output_dims); 0 != ret) {
                ALOGE("axclrtEngineGetOutputDims of output %d failed! ret=0x%08X", i, ret);
                return false;
            }
            this->output_tensor_shapes_.emplace_back(std::vector<int>(output_dims.dims, output_dims.dims + output_dims.dimCount));

            // clean memory, some cases model may need to clean memory
            // axclrtMemset(this->outputs_[i], 0, size);
        }

        // 10. create the IO
        if (const auto ret = axclrtEngineCreateIO(this->info_, &this->io_); 0 != ret) {
            ALOGE("Create model io failed{0x%08X}.", ret);
            return false;
        }
        // utilities::glog.print(utilities::log::type::info, "AXCLRT Engine inited.\n");

        // 11. set the input and output buffer
        for (uint32_t i = 0; i < input_count; i++) {
            if (const auto ret = axclrtEngineSetInputBufferByIndex(this->io_, i, this->inputs_[i], this->inputs_size_[i]); 0 != ret) {
                ALOGE("Set input buffer{index: %d} failed{0x%08X}.", i, ret);
                return false;
            }
        }
        for (uint32_t i = 0; i < output_count; i++) {
            if (const auto ret = axclrtEngineSetOutputBufferByIndex(this->io_, i, this->outputs_[i], this->outputs_size_[i]); 0 != ret) {
                ALOGE("Set output buffer{index: %d} failed{0x%08X}.", i, ret);
                return false;
            }
        }

        // 12. set the batch size
        if (const auto ret = axclrtEngineSetDynamicBatchSize(this->io_, this->batch_); 0 != ret) {
            ALOGE("Set batch size{%d} failed{0x%08X}.", this->batch_, ret);
            return false;
        }

        return true;
    }

private:
    std::unique_ptr<AxclEngineGuard> m_engine_guard;
    bool m_loaded = false;
    uint64_t model_id_ = 0;
    uint64_t context_id_ = 0;
    axclrtEngineIOInfo info_{};
    axclrtEngineIO io_{};
    int32_t group_ = 0;
    uint32_t batch_ = 0;
    int m_input_num;
    int m_output_num;
    AX_IO_BUFFER_STRATEGY_T m_strategy;

    std::vector<void*> inputs_;
    std::vector<void*> outputs_;

    std::vector<std::vector<int32_t>> input_tensor_shapes_;
    std::vector<std::vector<int32_t>> output_tensor_shapes_;

    std::vector<uintmax_t> inputs_size_;
    std::vector<uintmax_t> outputs_size_;
};

#endif