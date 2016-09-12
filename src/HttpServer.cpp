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
 * File:   HttpServer.cpp
 * Author: akashche
 * 
 * Created on July 10, 2015, 7:23 PM
 */

#include <string>
#include <atomic>
#include <cstdlib>
#include <functional>

#include "staticlib/config.hpp"
#include "staticlib/httpserver.hpp"
#include "staticlib/containers/blocking_queue.hpp"

#include "JvmtiHttpException.hpp"
#include "HttpServer.hpp"

namespace jvmti_http {

namespace sh = staticlib::httpserver;

const uint32_t NUMBER_OF_ASIO_THREADS = 1;
const uint32_t QUEUE_MAX_SIZE = 1 << 20;
const std::string HANDLERS_PATH = "/jvmti/";
const std::string WEBAPP_PATH = "/webapp";

HttpServer::HttpServer(uint16_t port, JvmtiAccessor* ja, const std::string& webapp_zip_path) :
server(1, port),
queue(1 << 10),
ja(ja),
// todo: fixme        
webapp_resource(webapp_zip_path, WEBAPP_PATH) {
    auto handler = [this](sh::http_request_ptr& req, sh::tcp_connection_ptr& conn) {
        // pion specific response writer creation
        auto writer = sh::http_response_writer::create(conn, req);
        // reroute to worker
        this->queue.emplace(std::move(writer), req->get_resource().substr(HANDLERS_PATH.length()));
    };
    auto webapp_handler = std::bind(&ZipResource::handle, this->webapp_resource,
            std::placeholders::_1, std::placeholders::_2);
    try {
        server.add_handler("GET", HANDLERS_PATH, handler);
        server.add_handler("GET", WEBAPP_PATH, webapp_handler);
        server.start();
        running.test_and_set();
    } catch (const std::exception& e) {
        throw JvmtiHttpException(TRACEMSG(e.what()));
    }
}

void JNICALL HttpServer::jvmti_callback(jvmtiEnv* jvmti, JNIEnv* jni, void* user_data) {
    auto hs_ptr = reinterpret_cast<HttpServer*>(user_data);
    hs_ptr->read_from_queue(jvmti, jni);

}

void HttpServer::shutdown() {
    server.stop(false);
    running.clear();
    // todo: investigate why blocks
//    queue.unblock();
}

void HttpServer::read_from_queue(jvmtiEnv* jvmti, JNIEnv* jni) {
    while (running.test_and_set()) {
        detail::Query el{};
        bool success = queue.take(el);
        if (success) {
            try {
                auto resp = ja->process_query(jvmti, jni, el.get_property());
                el.get_writer()->write_move(std::move(resp));
            } catch (const std::exception& e) {
                el.get_writer() << TRACEMSG(e.what()) << "\n";
            }
            el.get_writer()->send();
        } else break;
    }
    running.clear();
}

} // namespace
