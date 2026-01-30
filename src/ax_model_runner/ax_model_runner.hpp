/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <memory>

enum AX_IO_BUFFER_STRATEGY_T {
    AX_IO_BUFFER_STRATEGY_DEFAULT = 0,
    AX_IO_BUFFER_STRATEGY_CACHED
};

class AxModelRunner {
public:
    AxModelRunner();

    ~AxModelRunner();

    int load_model(const char* model_path, AX_IO_BUFFER_STRATEGY_T strategy = AX_IO_BUFFER_STRATEGY_CACHED, int device_index = 0);

    int unload_model(void);

    int run(void);

    int set_input(int index, void* data);
    int set_inputs(const std::vector<void*>& datas);
    // use DMA to copy data between models if possible, fallback to normal memcpy otherwise.
    int set_input_dma(int dst_index, AxModelRunner& src_model, int src_index);

    int get_output(int index, void* data);
    int get_outputs(const std::vector<void*>& datas);

    int get_input_num(void);
    int get_output_num(void);

    void* get_input_ptr(int index);
    void* get_output_ptr(int index);

    uint64_t get_input_phy_addr(int index);
    uint64_t get_output_phy_addr(int index);

    const char* get_input_name(int index);
    const char* get_output_name(int index);

    int get_input_size(int index);
    int get_output_size(int index);

    std::vector<int> get_input_shape(int index);
    std::vector<int> get_output_shape(int index);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
