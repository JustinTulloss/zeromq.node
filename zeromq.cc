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
#include <list>

using namespace v8;
using namespace node;

namespace zmq {

static Persistent<String> receive_symbol;
static Persistent<String> error_symbol;
static Persistent<String> connect_symbol;

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
        NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

        receive_symbol = NODE_PSYMBOL("receive");
        connect_symbol = NODE_PSYMBOL("connect");
        error_symbol = NODE_PSYMBOL("error");

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

        return 0;//zmq_msg_close(&z_msg);
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

        socket->QueueOutgoingMessage(args[0]);
        socket->AddEvent(ZMQ_POLLOUT);

        return Undefined();
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
        AddEvent(ZMQ_POLLIN | ZMQ_POLLERR);
    }

    ~Socket () {
        assert(socket_ == NULL);
    }

private:

    void AddEvent (int flags) {
        events_ |= flags;

        // start this if we haven't already
        revents_ = 0;
        eio_custom(DoPoll, EIO_PRI_DEFAULT, AfterPoll, this);
        ev_ref(EV_DEFAULT_UC);
    }

    void RemoveEvent (int flags) {
        events_ ^= flags; //TODO: Fix.
    }

    void QueueOutgoingMessage(Local <Value> message) {
        Persistent<Value> p_message = Persistent<Value>::New(message);
        outgoing_.push_back(p_message);
    }

    static Socket * getSocket(const Arguments &args) {
        return ObjectWrap::Unwrap<Socket>(args.This());
    }

    static int DoPoll(eio_req *req) {
        Socket *socket = (Socket *) req->data;
        zmq_pollitem_t pollers[1];
        zmq_pollitem_t *poller = &pollers[0];
        poller->socket = socket->socket_;
        poller->events = socket->events_;
        zmq_poll(pollers, 1, -1);
        socket->revents_ = poller->revents;
        return 0;
    }

    static int AfterPoll(eio_req *req) {
        HandleScope scope;

        Socket *socket = (Socket *)req->data;
        Local <Value> exception;

        if (socket->revents_ & ZMQ_POLLIN) {
            zmq_msg_t z_msg;
            if (socket->Recv(0, &z_msg)) {
                socket->Emit(error_symbol, 1, &exception);
            }
            Local <Value>js_msg = String::New(
                (char *) zmq_msg_data(&z_msg),
                zmq_msg_size(&z_msg));
            socket->Emit(receive_symbol, 1, &js_msg);
            zmq_msg_close(&z_msg);

        }

        if (socket->revents_ & ZMQ_POLLOUT && !socket->outgoing_.empty()) {
            String::Utf8Value message(socket->outgoing_.front());
            if (socket->Send(*message, message.length(), 0)) {
                exception = Exception::Error(
                    String::New(socket->ErrorMessage()));
                socket->Emit(receive_symbol, 1, &exception);
            }

            socket->outgoing_.front().Dispose();
            socket->outgoing_.pop_front();

            if (socket->outgoing_.empty()) {
                socket->RemoveEvent(ZMQ_POLLOUT);
            }
        }

        if (socket->revents_ & ZMQ_POLLERR) {
            exception = Exception::Error(
                String::New(socket->ErrorMessage()));
            socket->Emit(receive_symbol, 1, &exception);
        }

        socket->revents_ = 0;

        if (socket->events_) {
            eio_custom(DoPoll, EIO_PRI_DEFAULT, AfterPoll, socket);
        }
        else {
            ev_unref(EV_DEFAULT_UC);
        }

        return 0;
    }

    void *socket_;
    short revents_;
    short events_;
    ev_prepare watcher_;
    std::list< Persistent <Value> > outgoing_;
};

}

extern "C" void
init (Handle<Object> target) {
    HandleScope scope;
    zmq::Context::Initialize(target);
    zmq::Socket::Initialize(target);
}
