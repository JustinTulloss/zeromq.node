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
#include <ev.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <zmq.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <list>
#include <vector>
#include <stdexcept>

using namespace v8;
using namespace node;

#define SET_SOCKET_ACCESSOR(name)                                         \
do {                                                                      \
    t->PrototypeTemplate()->SetAccessor(String::New(name),                \
            GetOptions, SetOptions);                                      \
} while (0)


// FIXME: Error checking needs to be implemented on all `zmq_` calls.

namespace zmq {

class Context;
class OutgoingMessage;
class IncomingMessage;
class Socket;

static Persistent<String> message_symbol;
static Persistent<String> error_symbol;
static Persistent<String> connect_symbol;


static inline const char* ErrorMessage() {
    return zmq_strerror(zmq_errno());
}

static inline Local<Value> ExceptionFromError() {
    return Exception::Error(String::New(ErrorMessage()));
}


class Context : public EventEmitter {
friend class Socket;
public:
    static void Initialize(v8::Handle<v8::Object> target) {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(New);

        t->Inherit(EventEmitter::constructor_template);
        t->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

        target->Set(String::NewSymbol("Context"), t->GetFunction());
    }

    void Close() {
        zmq_term(context_);
        context_ = NULL;
        Unref();
    }

protected:
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;
        int io_threads = 1;
        if (args.Length() == 1) {
            if (!args[0]->IsNumber()) {
                return ThrowException(Exception::TypeError(
                    String::New("io_threads must be an integer")));
            }
            io_threads = (int) args[0]->ToInteger()->Value();
            if (io_threads < 1) {
                return ThrowException(Exception::RangeError(
                    String::New("io_threads must be a positive number")));
            }
        }

        Context *context = new Context(io_threads);
        context->Wrap(args.This());

        return args.This();
    }

    static Handle<Value> Close(const Arguments& args) {
        zmq::Context *context = ObjectWrap::Unwrap<Context>(args.This());
        HandleScope scope;
        context->Close();
        return Undefined();
    }

    Context(int io_threads) : EventEmitter() {
        context_ = zmq_init(io_threads);
    }

    ~Context() {
        if (context_ != NULL)
            Close();
        assert(context_ == NULL);
    }

private:
    void * context_;
};


/*
 * An object that creates a ØMQ message from the given Buffer Object,
 * and manages the reference to it using RAII. A persistent V8 handle
 * for the Buffer object will remain while its data is in use by ØMQ.
 */
class OutgoingMessage {
public:
    inline OutgoingMessage(Handle<Object> buf) {
        bufref_ = new BufferReference(buf);
        if (zmq_msg_init_data(&msg_, Buffer::Data(buf), Buffer::Length(buf),
                BufferReference::FreeCallback, bufref_) < 0) {
            delete bufref_;
            throw std::runtime_error(ErrorMessage());
        }
    };

    inline ~OutgoingMessage() {
        if (zmq_msg_close(&msg_) < 0)
            throw std::runtime_error(ErrorMessage());
    };

    inline operator zmq_msg_t*() {
        return &msg_;
    }

private:
    class BufferReference {
    public:
        inline BufferReference(Handle<Object> buf) {
            buf_ = Persistent<Object>::New(buf);
        }

        inline ~BufferReference() {
            buf_.Dispose();
        }

        static void FreeCallback(void* data, void* message) {
            delete (BufferReference*) message;
        }

    private:
        Persistent<Object> buf_;
    };

private:
    zmq_msg_t msg_;
    BufferReference* bufref_;
};


/*
 * An object that creates an empty ØMQ message, which can be used for
 * zmq_recv. After the receive call, a Buffer object wrapping the ØMQ
 * message can be requested. The reference for the ØMQ message will
 * remain while the data is in use by the Buffer.
 */
class IncomingMessage {
public:
    inline IncomingMessage() {
        msgref_ = new MessageReference();
    };

    inline ~IncomingMessage() {
        if (buf_.IsEmpty())
            delete msgref_;
        else
            buf_.Dispose();
    };

    inline operator zmq_msg_t*() {
        return *msgref_;
    }

    inline operator Local<Value>() {
        if (buf_.IsEmpty()) {
            Buffer* buf_obj = Buffer::New(
              (char*)zmq_msg_data(*msgref_), zmq_msg_size(*msgref_),
              FreeCallback, msgref_);
            buf_ = Persistent<Object>::New(buf_obj->handle_);
        }
        return Local<Value>::New(buf_);
    }

private:
    static void FreeCallback(char* data, void* message) {
        delete (MessageReference*) message;
    }

private:
    class MessageReference {
    public:
        inline MessageReference() {
            if (zmq_msg_init(&msg_) < 0)
                throw std::runtime_error(ErrorMessage());
        }

        inline ~MessageReference() {
            if (zmq_msg_close(&msg_) < 0)
                throw std::runtime_error(ErrorMessage());
        }

        inline operator zmq_msg_t*() {
            return &msg_;
        }

    private:
        zmq_msg_t msg_;
    };

private:
    Persistent<Object> buf_;
    MessageReference* msgref_;
};


class Socket : public EventEmitter {
public:
    static void Initialize(v8::Handle<v8::Object> target) {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(New);

        t->Inherit(EventEmitter::constructor_template);
        t->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_DEFINE_CONSTANT(t, ZMQ_PUB);
        NODE_DEFINE_CONSTANT(t, ZMQ_SUB);
        NODE_DEFINE_CONSTANT(t, ZMQ_REQ);
        NODE_DEFINE_CONSTANT(t, ZMQ_XREQ);
        NODE_DEFINE_CONSTANT(t, ZMQ_REP);
        NODE_DEFINE_CONSTANT(t, ZMQ_XREP);
        NODE_DEFINE_CONSTANT(t, ZMQ_PUSH);
        NODE_DEFINE_CONSTANT(t, ZMQ_PULL);
        NODE_DEFINE_CONSTANT(t, ZMQ_PAIR);

        NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
        NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
        NODE_SET_PROTOTYPE_METHOD(t, "subscribe", Subscribe);
        NODE_SET_PROTOTYPE_METHOD(t, "unsubscribe", Unsubscribe);
        NODE_SET_PROTOTYPE_METHOD(t, "send", Send);
        NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

        SET_SOCKET_ACCESSOR("highwaterMark");
        SET_SOCKET_ACCESSOR("diskOffloadSize");
        SET_SOCKET_ACCESSOR("identity");
        SET_SOCKET_ACCESSOR("multicastDataRate");
        SET_SOCKET_ACCESSOR("recoveryIVL");
        SET_SOCKET_ACCESSOR("multicastLoop");
        SET_SOCKET_ACCESSOR("sendBufferSize");
        SET_SOCKET_ACCESSOR("receiveBufferSize");

        message_symbol = NODE_PSYMBOL("message");
        connect_symbol = NODE_PSYMBOL("connect");
        error_symbol = NODE_PSYMBOL("error");

        target->Set(String::NewSymbol("Socket"), t->GetFunction());
    }

    int Bind(const char *address) {
        return zmq_bind(socket_, address);
    }

    int Connect(const char *address) {
        return zmq_connect(socket_, address);
    }

    Handle<Value> GetLongSockOpt(int option) {
        int64_t value = 0;
        size_t len = sizeof(value);
        zmq_getsockopt(socket_, option, &value, &len);
        return v8::Integer::New(value); // WARNING: long cast to int!
    }

    Handle<Value> SetLongSockOpt(int option, Local<Value> wrappedValue) {
        if (!wrappedValue->IsNumber()) {
            return ThrowException(Exception::TypeError(
                String::New("Value must be an integer")));
        }
        int64_t value = (int64_t) wrappedValue->ToInteger()->Value(); // WARNING: int cast to long!
        zmq_setsockopt(socket_, option, &value, sizeof(value));
        return Undefined();
    }

    Handle<Value> GetULongSockOpt(int option) {
        uint64_t value = 0;
        size_t len = sizeof(value);
        zmq_getsockopt(socket_, option, &value, &len);
        return v8::Integer::New(value); // WARNING: long cast to int!
    }

    Handle<Value> SetULongSockOpt(int option, Local<Value> wrappedValue) {
        if (!wrappedValue->IsNumber()) {
            return ThrowException(Exception::TypeError(
                String::New("Value must be an integer")));
        }
        uint64_t value = (uint64_t) wrappedValue->ToInteger()->Value(); // WARNING: int cast to long!
        zmq_setsockopt(socket_, option, &value, sizeof(value));
        return Undefined();
    }

    Handle<Value> GetBytesSockOpt(int option) {
        char value[1024] = {0};
        size_t len = 1023;
        zmq_getsockopt(socket_, option, value, &len);
        return v8::String::New(value);
    }

    Handle<Value> SetBytesSockOpt(int option, Local<Value> wrappedValue) {
        if (!wrappedValue->IsString()) {
            return ThrowException(Exception::TypeError(
                String::New("Value must be a string!")));
        }
        String::Utf8Value value(wrappedValue->ToString());
        zmq_setsockopt(socket_, option, *value, value.length());
        return Undefined();
    }

    void Close() {
        ev_io_stop(EV_DEFAULT_UC_ &watcher_);
        zmq_close(socket_);
        socket_ = NULL;
        Unref();
    }

protected:
    static Handle<Value> New(const Arguments &args) {
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

    static Handle<Value> Connect(const Arguments &args) {
        HandleScope scope;
        Socket *socket = GetSocket(args);
        if (!args[0]->IsString()) {
            return ThrowException(Exception::TypeError(
                String::New("Address must be a string!")));
        }

        String::Utf8Value address(args[0]->ToString());
        if (socket->Connect(*address)) {
            return ThrowException(ExceptionFromError());
        }
        return Undefined();
    }

    static Handle<Value> Subscribe(const Arguments &args) {
        return GetSocket(args)->SetBytesSockOpt(ZMQ_SUBSCRIBE, args[0]);
    }

    static Handle<Value> Unsubscribe(const Arguments &args) {
        return GetSocket(args)->SetBytesSockOpt(ZMQ_UNSUBSCRIBE, args[0]);
    }

    static Handle<Value> GetOptions(Local<String> name, const AccessorInfo& info) {
        Socket *socket = GetSocket(info);

        if (name->Equals(v8::String::New("highwaterMark"))) {
          return socket->GetULongSockOpt(ZMQ_HWM);
        } else if (name->Equals(v8::String::New("diskOffloadSize"))) {
          return socket->GetLongSockOpt(ZMQ_SWAP);
        } else if (name->Equals(v8::String::New("identity"))) {
            return socket->GetBytesSockOpt(ZMQ_IDENTITY);
        } else if (name->Equals(v8::String::New("multicastDataRate"))) {
            return socket->GetLongSockOpt(ZMQ_RATE);
        } else if (name->Equals(v8::String::New("recoveryIVL"))) {
            return socket->GetLongSockOpt(ZMQ_RECOVERY_IVL);
        } else if (name->Equals(v8::String::New("multicastLoop"))) {
            return socket->GetLongSockOpt(ZMQ_MCAST_LOOP);
        } else if (name->Equals(v8::String::New("sendBufferSize"))) {
            return socket->GetULongSockOpt(ZMQ_SNDBUF);
        } else if (name->Equals(v8::String::New("receiveBufferSize"))) {
            return socket->GetULongSockOpt(ZMQ_RCVBUF);
        }

        return Undefined();
    }

    static void SetOptions(Local<String> name, Local<Value> value, const AccessorInfo& info) {
        Socket *socket = GetSocket(info);

        if (name->Equals(v8::String::New("highwaterMark"))) {
          socket->SetULongSockOpt(ZMQ_HWM, value);
        } else if (name->Equals(v8::String::New("diskOffloadSize"))) {
          socket->SetLongSockOpt(ZMQ_SWAP, value);
        } else if (name->Equals(v8::String::New("identity"))) {
            socket->SetBytesSockOpt(ZMQ_IDENTITY, value);
        } else if (name->Equals(v8::String::New("multicastDataRate"))) {
            socket->SetLongSockOpt(ZMQ_RATE, value);
        } else if (name->Equals(v8::String::New("recoveryIVL"))) {
            socket->SetLongSockOpt(ZMQ_RECOVERY_IVL, value);
        } else if (name->Equals(v8::String::New("multicastLoop"))) {
            socket->SetLongSockOpt(ZMQ_MCAST_LOOP, value);
        } else if (name->Equals(v8::String::New("sendBufferSize"))) {
            socket->SetULongSockOpt(ZMQ_SNDBUF, value);
        } else if (name->Equals(v8::String::New("receiveBufferSize"))) {
            socket->SetULongSockOpt(ZMQ_RCVBUF, value);
        }
    }

    static Handle<Value> Bind(const Arguments &args) {
        HandleScope scope;
        Socket *socket = GetSocket(args);
        Local<Function> cb = Local<Function>::Cast(args[1]);
        if (!args[0]->IsString()) {
            return ThrowException(Exception::TypeError(
                String::New("Address must be a string!")));
        }

        if (args.Length() > 1 && !args[1]->IsFunction()) {
            return ThrowException(Exception::TypeError(
                String::New("Provided callback must be a function")));
        }

        socket->bindCallback_ = Persistent<Function>::New(cb);
        socket->bindAddress_ = Persistent<String>::New(args[0]->ToString());
        eio_custom(EIO_DoBind, EIO_PRI_DEFAULT, EIO_BindDone, socket);

        socket->Ref(); // Reference ourself until the callback is done
        ev_ref(EV_DEFAULT_UC);

        return Undefined();
    }

    static Handle<Value> Send(const Arguments &args) {
        HandleScope scope;
        Socket *socket = GetSocket(args);
        int argc = args.Length();

        if (argc == 0) {
            return ThrowException(Exception::TypeError(
                String::New("Must pass in at least one message part to send")));
        }

        for (int i = 0; i < argc; ++i) {
            Local<Value> arg = args[i];
            if (!Buffer::HasInstance(arg)) {
                return ThrowException(Exception::TypeError(
                    String::New("All message parts must be Buffers")));
            }
        }

        Local<Array> p_message = Array::New(argc);
        for (int i = 0; i < argc; ++i) {
            p_message->Set(i, args[i]);
        }
        socket->events_ |= ZMQ_POLLOUT;
        socket->outgoing_.push_back(Persistent<Array>::New(p_message));
        socket->AfterPoll();

        return Undefined();
    }

    static Handle<Value> Close(const Arguments &args) {
        HandleScope scope;

        Socket *socket = GetSocket(args);
        socket->Close();
        return Undefined();
    }

    Socket(Context *context, int type) : EventEmitter () {
        socket_ = zmq_socket(context->context_, type);
        events_ = ZMQ_POLLIN;

        size_t zmq_fd_size = sizeof(int);
        int fd;
        zmq_getsockopt(socket_, ZMQ_FD, &fd, &zmq_fd_size);

        ev_init(&watcher_, Socket::Callback);
        ev_io_set(&watcher_, fd, EV_READ);
        watcher_.data = this;
        ev_io_start(EV_DEFAULT_UC_ &watcher_);
    }

    ~Socket() {
        if (socket_ != NULL) {
            Close();
        }
        assert(socket_ == NULL);
    }

private:
    static void Callback(EV_P_ ev_io *w, int ev_revents) {
        Socket *s = static_cast<Socket*>(w->data);

        s->AfterPoll();
    }

    uint32_t CurrentEvents() {
        uint32_t result;
        size_t result_size = sizeof (result);
        zmq_getsockopt(socket_, ZMQ_EVENTS, &result, &result_size);

        return result;
    }

    int64_t RcvMore() {
        int64_t result;
        size_t result_size = sizeof (result);
        zmq_getsockopt(socket_, ZMQ_RCVMORE, &result, &result_size);

        return result;
    }

    int AfterPoll() {
        HandleScope scope;


        while (CurrentEvents() & ZMQ_POLLIN) {
            std::vector< Local<Value> > argv;
            do {
                IncomingMessage im;
                if (zmq_recv(socket_, im, 0) < 0) {
                    Local<Value> exception = ExceptionFromError();
                    Emit(error_symbol, 1, &exception);
                }
                else
                    argv.push_back(im);
            } while(RcvMore());
            Emit(message_symbol, argv.size(), &argv[0]);
        }

        while ((CurrentEvents() & ZMQ_POLLOUT) && !outgoing_.empty()) {
            Local<Array> parts = Local<Array>::New(outgoing_.front());
            uint32_t len = parts->Length();
            for (uint32_t i = 0; i < len; ++i) {
                OutgoingMessage msg(parts->Get(i)->ToObject());
                int flags = (i == (len-1)) ? 0 : ZMQ_SNDMORE;
                if (zmq_send(socket_, msg, flags) < 0) {
                    Local<Value> exception = ExceptionFromError();
                    Emit(error_symbol, 1, &exception);
                }
            }
            // TODO: document the consequences of disposing even when there's an
            // error in transmission
            outgoing_.front().Dispose();
            outgoing_.pop_front();
        }

        return 0;
    }

    static void ReleaseReceivedMessage(char *data, void *hint) {
        zmq_msg_t *z_msg = (zmq_msg_t *) hint;
        zmq_msg_close(z_msg);
        delete z_msg;
    }

    static int EIO_DoBind(eio_req *req) {
        Socket *socket = (Socket *) req->data;
        String::Utf8Value address(socket->bindAddress_);
        socket->bindError_ = socket->Bind(*address);
        return 0;
    }

    static int EIO_BindDone(eio_req *req) {
        HandleScope scope;

        ev_unref(EV_DEFAULT_UC);

        Socket *socket = (Socket *) req->data;

        TryCatch try_catch;

        Local<Value> argv[1];

        if (socket->bindError_)
            argv[0] = ExceptionFromError();
        else
            argv[0] = Local<Value>::New(Undefined());
        socket->bindCallback_->Call(v8::Context::GetCurrent()->Global(), 1, argv);

        if (try_catch.HasCaught())
            FatalException(try_catch);

        socket->bindAddress_.Dispose();
        socket->bindCallback_.Dispose();

        socket->Unref();

        return 0;
    }

    static Socket * GetSocket(const Arguments &args) {
        return ObjectWrap::Unwrap<Socket>(args.This());
    }

    static Socket * GetSocket(const AccessorInfo &info) {
        return ObjectWrap::Unwrap<Socket>(info.This());
    }

    void *socket_;
    ev_io watcher_;
    short events_;
    std::list< Persistent<Array> > outgoing_;

    Persistent<String> bindAddress_;
    Persistent<Function> bindCallback_;
    int bindError_;
};


}

extern "C" void
init(Handle<Object> target) {
    HandleScope scope;
    zmq::Context::Initialize(target);
    zmq::Socket::Initialize(target);
}
