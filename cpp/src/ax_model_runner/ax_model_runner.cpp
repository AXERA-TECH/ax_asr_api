/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include "ax_model_runner/ax_model_runner.hpp"
#include "utils/logger.h"
#include "utils/memory_utils.hpp"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vector>
#include <cstdint>

#if defined (CHIP_AX650) || defined(CHIP_AX630C) || defined(CHIP_AX620Q)
    #include "ax_engine_impl.hpp"
#elif defined (CHIP_AX8850)
    #include "axcl_engine_impl.hpp"
#else
    #error Unknown CHIP_TYPE, check cmake/msp_dependencies.cmake for possible choices
#endif


AxModelRunner::AxModelRunner():
    impl_(std::make_unique<Impl>()) {

}

AxModelRunner::~AxModelRunner() {
    impl_->unload_model();
}

int AxModelRunner::load_model(const char* model_path, AX_IO_BUFFER_STRATEGY_T strategy, int device_index) {
    return impl_->load_model(model_path, strategy, device_index);
}

int AxModelRunner::unload_model(void) {
    return impl_->unload_model();
}

int AxModelRunner::run(void) {
    return impl_->run();
}

int AxModelRunner::set_input(int index, void* data) {
    return impl_->set_input(index, data);
}

int AxModelRunner::set_inputs(const std::vector<void*>& datas) {
    return impl_->set_inputs(datas);
}

int AxModelRunner::set_input_dma(int dst_index, AxModelRunner& src_model, int src_index) {
    return impl_->set_input_dma(dst_index, src_model, src_index);
}

int AxModelRunner::get_output(int index, void* data) {
    return impl_->get_output(index, data);
}

int AxModelRunner::get_outputs(const std::vector<void*>& datas) {
    return impl_->get_outputs(datas);
}

void* AxModelRunner::get_input_ptr(int index) {
    return impl_->get_input_ptr(index);
}

void* AxModelRunner::get_output_ptr(int index) {
    return impl_->get_output_ptr(index);
}

uint64_t AxModelRunner::get_input_phy_addr(int index) {
    return impl_->get_input_phy_addr(index);
}

uint64_t AxModelRunner::get_output_phy_addr(int index) {
    return impl_->get_output_phy_addr(index);
}

const char* AxModelRunner::get_input_name(int index) {
    return impl_->get_input_name(index);
}

const char* AxModelRunner::get_output_name(int index) {
    return impl_->get_output_name(index);
}

int AxModelRunner::get_input_size(int index) {
    return impl_->get_input_size(index);
}

int AxModelRunner::get_output_size(int index) {
    return impl_->get_output_size(index);
}

std::vector<int> AxModelRunner::get_input_shape(int index) {
    return impl_->get_input_shape(index);
}

std::vector<int> AxModelRunner::get_output_shape(int index) {
    return impl_->get_output_shape(index);
}