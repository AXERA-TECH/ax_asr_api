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

#include <string.h>
#include <stdlib.h>
#include <wchar.h> 
#include <assert.h>
#include <iostream>

// uint32 base64_encode(char* input, uint8* encode);
int base64_decode(const uint8_t* code, uint32_t code_len, char* str);