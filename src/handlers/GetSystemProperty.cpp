/*
 * Copyright 2015, akashche at redhat.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   GetSystemProperty.cpp
 * Author: akashche
 *
 * Created on July 12, 2015, 6:41 PM
 */

#include "handlers.hpp"

std::string handle_GetSystemProperty(jvmtiEnv* jvmti, JNIEnv* /* jni */, const std::string& input) {
    char* buf = nullptr;
    auto error = jvmti->GetSystemProperty(input.c_str(), &buf);
    auto error_str = to_error_message(jvmti, error);
    if (!error_str.empty()) {
        throw std::runtime_error(TRACEMSG(error_str));
    }
    return std::string{buf};
}
