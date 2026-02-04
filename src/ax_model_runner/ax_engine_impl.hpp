/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#if defined (CHIP_AX650) || defined(CHIP_AX630C) || defined(CHIP_AX620Q)

#pragma once

#include <vector>
#include <string>
#include <string.h>
#include <ax_sys_api.h>

#include "ax_model_runner.hpp"
#include "ax_engine_api.h"
#include "ax_engine_guard.hpp"
#include "utils/memory_utils.hpp"
#include "utils/logger.h"

#if defined (CHIP_AX650)
    #include "ax_dmadim_api.h"
#endif

#define AX_IO_CMM_ALIGN_SIZE   128

class AxModelRunner::Impl {
public:
    Impl():
        m_handle(nullptr),
        m_pIOinfo(nullptr),
        m_input_num(0),
        m_output_num(0),
        m_loaded(false) {

        memset(&m_io, 0, sizeof(AX_ENGINE_IO_T));
    }

    ~Impl() {
        unload_model();
    }

    int load_model(const char* model_path, AX_IO_BUFFER_STRATEGY_T strategy, int device_index) {
        if (!utils::file_exist(std::string(model_path))) {
            ALOGE("model path %s not exist!", model_path);
            return -1;
        }

        AX_CHAR *pModelBufferVirAddr = nullptr;
        AX_U32 nModelBufferSize = 0;
            
        MMap model_buffer(model_path);
        pModelBufferVirAddr = (char*)model_buffer.data();
        nModelBufferSize = model_buffer.size();

        auto freeModelBuffer = [&]() {
            model_buffer.close_file();
            return;
        };

        int ret = AX_ENGINE_CreateHandle(&m_handle, pModelBufferVirAddr, nModelBufferSize);
        if (0 != ret) {
            ALOGE("AX_ENGINE_CreateHandle failed! ret=0x%x", ret);
            freeModelBuffer();
            return ret;
        }
            
        ret = AX_ENGINE_CreateContext(m_handle);
        if (0 != ret) {
            ALOGE("AX_ENGINE_CreateContext failed! ret=0x%x", ret);
            freeModelBuffer();
            return ret;
        }

        m_strategy = strategy;
        ret = _prepare_io();
        if (0 != ret) {
            ALOGE("_prepare_io failed! ret=0x%x", ret);
            freeModelBuffer();
            _free_io();
            return ret;
        }

        freeModelBuffer();
        m_loaded = (ret == 0);

        return ret;
    }

    int unload_model(void) {
        int ret = 0;
        if (m_handle != 0) {
            ALOGD("Detroy engine handle");
            ret = AX_ENGINE_DestroyHandle(m_handle);
            m_handle = 0;

            _free_io();
        }
        return ret;
    }

    int run(void) {
        if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED) {
            for (int index = 0; index < m_input_num; index++) {
                _cache_io_flush(m_io.pInputs[index]);
            }
        }

        int ret = AX_ENGINE_RunSync(m_handle, &m_io);
        if (0 != ret) {
            ALOGE("AX_ENGINE_RunSync failed! ret=0x%x", ret);
            return ret;
        }
        return ret;
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

        memcpy(m_io.pInputs[index].pVirAddr, data, m_io.pInputs[index].nSize);

        return 0;
    }

    int set_inputs(const std::vector<void*>& datas) {
        for (int index = 0; index < m_input_num; index++) {
            void* data = datas[index];
            if (!data) {
                ALOGE("index %d data is null", index);
                return -1;
            }

            memcpy(m_io.pInputs[index].pVirAddr, data, m_io.pInputs[index].nSize);
        }

        return 0;
    }

    int set_input_dma(int dst_index, AxModelRunner& src_model, int src_index) {
        #if defined (CHIP_AX650)
            AX_U64 phySrc = src_model.get_output_phy_addr(src_index);
            AX_U64 phyDst = this->get_input_phy_addr(dst_index);
            int size = src_model.get_output_size(src_index);

            int ret = AX_DMA_MemCopy(phyDst, phySrc, (AX_U64)size);
            if (ret) {
                ALOGW("AX_DMA_MemCopy failed! ret=0x%x, fallback to sys memcpy", ret);

                this->set_input(dst_index, src_model.get_output_ptr(src_index));
                return 0;
            }
        #else
            this->set_input(dst_index, src_model.get_output_ptr(src_index));
            return 0;
        #endif
    }

    int get_output(int index, void* data) {
        if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED)
            _cache_io_flush(m_io.pOutputs[index]);

        memcpy(data, m_io.pOutputs[index].pVirAddr, m_io.pOutputs[index].nSize);

        return 0;
    }

    int get_outputs(const std::vector<void*>& datas) {
        for (int index = 0; index < m_output_num; index++) {
            void* data = datas[index];
            if (!data) {
                ALOGE("index %d data is null", index);
                return -1;
            }

            if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED)
                _cache_io_flush(m_io.pOutputs[index]);

            memcpy(data, m_io.pOutputs[index].pVirAddr, m_io.pOutputs[index].nSize);
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
        return m_io.pInputs[index].pVirAddr;
    }

    void* get_output_ptr(int index) {
        if (m_strategy == AX_IO_BUFFER_STRATEGY_CACHED)
            _cache_io_flush(m_io.pOutputs[index]);

        return m_io.pOutputs[index].pVirAddr;
    }

    inline AX_U64 get_input_phy_addr(int index) {
        return m_io.pInputs[index].phyAddr;
    }

    inline AX_U64 get_output_phy_addr(int index) {
        return m_io.pOutputs[index].phyAddr;
    }

    inline const char* get_input_name(int index) {
        return m_input_names[index].c_str();
    }

    inline const char* get_output_name(int index) {
        return m_output_names[index].c_str();
    }

    inline int get_input_size(int index) {
        return m_pIOinfo->pInputs[index].nSize;
    }

    inline int get_output_size(int index) {
        return m_pIOinfo->pOutputs[index].nSize;
    }

    std::vector<int> get_input_shape(int index) {
        std::vector<int> shape;
        shape.resize(m_pIOinfo->pInputs[index].nShapeSize);
        for (int i = 0; i < shape.size(); i++) {
            shape[i] = m_pIOinfo->pInputs[index].pShape[i];
        }
        return shape;
    }

    std::vector<int> get_output_shape(int index) {
        std::vector<int> shape;
        shape.resize(m_pIOinfo->pOutputs[index].nShapeSize);
        for (int i = 0; i < shape.size(); i++) {
            shape[i] = m_pIOinfo->pOutputs[index].pShape[i];
        }
        return shape;
    }

private:
    int _prepare_io() {
        int ret = AX_ENGINE_GetIOInfo(m_handle, &m_pIOinfo);
        if (0 != ret) {
            ALOGE("AX_ENGINE_GetIOInfo failed! ret=0x%x", ret);
            return ret;
        }

        m_input_num = m_pIOinfo->nInputSize;
        m_output_num = m_pIOinfo->nOutputSize;

        m_io.nInputSize = m_pIOinfo->nInputSize;
        m_io.nOutputSize = m_pIOinfo->nOutputSize;

        m_io.pInputs = new AX_ENGINE_IO_BUFFER_T[m_pIOinfo->nInputSize];
        m_io.pOutputs = new AX_ENGINE_IO_BUFFER_T[m_pIOinfo->nOutputSize];

        for (int i = 0; i < m_pIOinfo->nInputSize; i++) {
            const char* layer_name = m_pIOinfo->pInputs[i].pName;
            m_input_names.push_back(std::string(layer_name));

            ret = _alloc_io_buffer(m_io.pInputs[i], m_pIOinfo->pInputs[i], m_strategy);
            if (0 != ret) {
                ALOGE("_alloc_io_buffer for input[%d] failed! ret=0x%x", i, ret);
                return ret;
            }
        }

        for (int i = 0; i < m_pIOinfo->nOutputSize; i++) {
            const char* layer_name = m_pIOinfo->pOutputs[i].pName;
            m_output_names.push_back(std::string(layer_name));

            ret = _alloc_io_buffer(m_io.pOutputs[i], m_pIOinfo->pOutputs[i], m_strategy);
            if (0 != ret) {
                ALOGE("_alloc_io_buffer for output[%d] failed! ret=0x%x", i, ret);
                return ret;
            }
        }

        return ret;
    }

    void _free_io() {
        for (size_t i = 0; i < m_io.nInputSize; i++) {
            if (0 != m_io.pInputs[i].phyAddr)
                AX_SYS_MemFree(m_io.pInputs[i].phyAddr, m_io.pInputs[i].pVirAddr);
        }

        for (size_t i = 0; i < m_io.nOutputSize; i++) {
            if (0 != m_io.pOutputs[i].phyAddr)
                AX_SYS_MemFree(m_io.pOutputs[i].phyAddr, m_io.pOutputs[i].pVirAddr);
        }
        
        delete[] m_io.pInputs;
        delete[] m_io.pOutputs;
        memset(&m_io, 0, sizeof(AX_ENGINE_IO_T));
    }

    int _alloc_io_buffer(AX_ENGINE_IO_BUFFER_T &buffer, 
            const AX_ENGINE_IOMETA_T &meta, AX_IO_BUFFER_STRATEGY_T strategy) {
        int ret = 0;
    
        memset(&buffer, 0, sizeof(AX_ENGINE_IO_BUFFER_T));
        buffer.nSize = meta.nSize;
        
        if (AX_IO_BUFFER_STRATEGY_DEFAULT == strategy) {
            AX_SYS_MemAlloc((AX_U64*)&buffer.phyAddr, 
                (AX_VOID**)&buffer.pVirAddr, 
                meta.nSize, AX_IO_CMM_ALIGN_SIZE, (const AX_S8*)meta.pName);
        } else {
            AX_SYS_MemAllocCached((AX_U64*)&buffer.phyAddr, 
                (AX_VOID**)&buffer.pVirAddr, 
                meta.nSize, AX_IO_CMM_ALIGN_SIZE, (const AX_S8*)meta.pName);
        }

        return ret;
    }

    void _cache_io_flush(AX_ENGINE_IO_BUFFER_T &buffer) {
        if (buffer.phyAddr != 0) {
            AX_SYS_MflushCache(buffer.phyAddr, buffer.pVirAddr, buffer.nSize);
        }
    }     
    
private:
    AX_ENGINE_HANDLE m_handle;
    AX_ENGINE_IO_T m_io;
    AX_ENGINE_IO_INFO_T* m_pIOinfo;
    int m_input_num;
    int m_output_num;
    AX_IO_BUFFER_STRATEGY_T m_strategy;
    std::vector<std::string> m_input_names;
    std::vector<std::string> m_output_names;
    bool m_loaded;
    AxEngineGuard m_engine_guard;
};

#endif