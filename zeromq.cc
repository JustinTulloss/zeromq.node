/*
 * Copyright (c) 2010 Justin Tulloss
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <zmq.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

using namespace v8;
using namespace node;

namespace zmq {

class Context : public EventEmitter {
public:
    static void
    Initialize (v8::Handle<v8::Object> target) {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(New);

        t->Inherit(EventEmitter::constructor_template);
        t->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

        target->Set(String::NewSymbol("Context"), t->GetFunction());
    }

    void * getCContext() {
        return context_;
    }

    void Close(Local<Value> exception = Local<Value>()) {
        zmq_term(context_);
        context_ = NULL;
        Unref();
    }

protected:
    static Handle<Value>
    New (const Arguments& args) {
        HandleScope scope;

        Context *context = new Context();
        context->Wrap(args.This());

        return args.This();
    }

    static Handle<Value>
    Close (const Arguments& args) {
        zmq::Context *context = ObjectWrap::Unwrap<Context>(args.This());
        HandleScope scope;
        context->Close();
        return Undefined();
    }

    Context () : EventEmitter () {
        context_ = zmq_init(1);
    }

    ~Context () {
        assert(context_ == NULL);
    }

private:
    void * context_;
};


class Socket : public EventEmitter {
public:
    static void
    Initialize (v8::Handle<v8::Object> target) {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(New);

        t->Inherit(EventEmitter::constructor_template);
        t->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_DEFINE_CONSTANT(t, ZMQ_PUB);
        NODE_DEFINE_CONSTANT(t, ZMQ_SUB);
        NODE_DEFINE_CONSTANT(t, ZMQ_REQ);
        NODE_DEFINE_CONSTANT(t, ZMQ_REP);
        NODE_DEFINE_CONSTANT(t, ZMQ_UPSTREAM);
        NODE_DEFINE_CONSTANT(t, ZMQ_DOWNSTREAM);
        NODE_DEFINE_CONSTANT(t, ZMQ_PAIR);

        NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
        NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
        NODE_SET_PROTOTYPE_METHOD(t, "send", Send);
        NODE_SET_PROTOTYPE_METHOD(t, "recv", Recv);
        NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

        target->Set(String::NewSymbol("Socket"), t->GetFunction());
    }

    int Bind(const char *address) {
        return zmq_bind(socket_, address) == 0;
    }

    int Connect(const char *address) {
        return zmq_connect(socket_, address) == 0;
    }

    int Send(char *msg, int length, int flags) {
        int rc;
        zmq_msg_t z_msg;
        rc = zmq_msg_init_data(&z_msg, msg, length, NULL, NULL);
        if (rc < 0) {
            return rc;
        }

        rc = zmq_send(socket_, &z_msg, flags);
        if (rc < 0) {
            return rc;
        }

        return zmq_msg_close(&z_msg);
    }

    int Recv(int flags, zmq_msg_t* z_msg) {
        int rc;
        rc = zmq_msg_init(z_msg);
        if (rc < 0) {
            return rc;
        }
        return zmq_recv(socket_, z_msg, flags);
    }

    void Close(Local<Value> exception = Local<Value>()) {
        zmq_close(socket_);
        socket_ = NULL;
        Unref();
    }

    const char * ErrorMessage() {
        return zmq_strerror(zmq_errno());
    }

protected:
    static Handle<Value>
    New (const Arguments &args) {
        HandleScope scope;
        if (args.Length() != 2) {
            return ThrowException(Exception::Error(
                String::New("Must pass a context and a type to constructor")));
        }
        Context *context = ObjectWrap::Unwrap<Context>(args[0]->ToObject());
        if (!args[1]->IsNumber()) {
            return ThrowException(Exception::TypeError(
                String::New("Type must be an integer")));
        }
        int type = (int) args[1]->ToInteger()->Value();

        Socket *socket = new Socket(context, type);
        socket->Wrap(args.This());

        return args.This();
    }

    static Handle<Value>
    Connect (const Arguments &args) {
        HandleScope scope;
        Socket *socket = getSocket(args);
        if (!args[0]->IsString()) {
            return ThrowException(Exception::TypeError(
                String::New("Address must be a string!")));
        }

        String::Utf8Value address(args[0]->ToString());
        if (!socket->Connect(*address)) {
            return ThrowException(Exception::Error(
                String::New(socket->ErrorMessage())));
        }
        return Undefined();
    }

    static Handle<Value>
    Bind (const Arguments &args) {
        HandleScope scope;
        Socket *socket = getSocket(args);
        if (!args[0]->IsString()) {
            return ThrowException(Exception::TypeError(
                String::New("Address must be a string!")));
        }

        String::Utf8Value address(args[0]->ToString());
        if (!socket->Bind(*address)) {
            return ThrowException(Exception::Error(
                String::New(socket->ErrorMessage())));
        }
        return Undefined();
    }

    static Handle<Value>
    Send (const Arguments &args) {
        HandleScope scope;
        Socket *socket = getSocket(args);
        if (!args.Length() == 1) {
            return ThrowException(Exception::TypeError(
                String::New("Must pass in a string to send")));
        }

        String::Utf8Value message(args[0]);
        if (socket->Send(*message, message.length(), 0)) {
            return ThrowException(Exception::Error(
                String::New(socket->ErrorMessage())));
        }
        return Undefined();
    }

    static Handle<Value>
    Recv (const Arguments &args) {
        HandleScope scope;

        Socket *socket = getSocket(args);
        zmq_msg_t z_msg;
        if (socket->Recv(0, &z_msg)) {
            return ThrowException(Exception::Error(
                String::New(socket->ErrorMessage())));
        }
        Local <String>js_msg = String::New(
            (char *) zmq_msg_data(&z_msg),
            zmq_msg_size(&z_msg));
        zmq_msg_close(&z_msg);
        return scope.Close(js_msg);
    }

    static Handle<Value>
    Close (const Arguments &args) {
        HandleScope scope;

        Socket *socket = getSocket(args);
        socket->Close();
        return Undefined();
    }

    Socket (Context *context, int type) : EventEmitter () {
        socket_ = zmq_socket(context->getCContext(), type);
    }

    ~Socket () {
        assert(socket_ == NULL);
    }

private:
    static Socket * getSocket(const Arguments &args) {
        return ObjectWrap::Unwrap<Socket>(args.This());
    }
    void *socket_;
};

}

extern "C" void
init (Handle<Object> target) {
    HandleScope scope;
    zmq::Context::Initialize(target);
    zmq::Socket::Initialize(target);
}
