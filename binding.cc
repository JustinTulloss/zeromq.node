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
#include <stdexcept>

using namespace v8;
using namespace node;

// FIXME: Error checking needs to be implemented on all `zmq_` calls.

namespace zmq {



class Socket;

class Context : public EventEmitter {
friend class Socket;
public:
    static void Initialize(v8::Handle<v8::Object> target);

    virtual ~Context();

private:
    static Handle<Value> New(const Arguments& args);
    Context(int io_threads);
    static Context* GetContext(const Arguments &args);

    void Close();
    static Handle<Value> Close(const Arguments& args);

    void* context_;
};


class Socket : public EventEmitter {
public:
    static void Initialize(v8::Handle<v8::Object> target);

    virtual ~Socket();

private:
    static Handle<Value> New(const Arguments &args);
    Socket(Context *context, int type);
    static Socket* GetSocket(const Arguments &args);

    static void Callback(EV_P_ ev_io *w, int ev_revents);

    template<typename T>
    Handle<Value> GetSockOpt(int option);
    template<typename T>
    Handle<Value> SetSockOpt(int option, Handle<Value> wrappedValue);
    static Handle<Value> GetSockOpt(const Arguments &args);
    static Handle<Value> SetSockOpt(const Arguments &args);

    struct BindState;
    static Handle<Value> Bind(const Arguments &args);
    static int EIO_DoBind(eio_req *req);
    static int EIO_BindDone(eio_req *req);

    static Handle<Value> Connect(const Arguments &args);

    class IncomingMessage;
    static Handle<Value> Recv(const Arguments &args);

    class OutgoingMessage;
    static Handle<Value> Send(const Arguments &args);

    void Close();
    static Handle<Value> Close(const Arguments &args);

    void *socket_;
    ev_io watcher_;
};



// --- I m p l e m e n t a t i o n ---


/*
 * Helpers for dealing with ØMQ errors.
 */

static inline const char* ErrorMessage() {
    return zmq_strerror(zmq_errno());
}

static inline Local<Value> ExceptionFromError() {
    return Exception::Error(String::New(ErrorMessage()));
}



/*
 * Context methods.
 */

void Context::Initialize(v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

    target->Set(String::NewSymbol("Context"), t->GetFunction());
}


Context::~Context() {
    Close();
}


Handle<Value> Context::New(const Arguments& args) {
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

Context::Context(int io_threads) : EventEmitter() {
    context_ = zmq_init(io_threads);
}

Context* Context::GetContext(const Arguments &args) {
    return ObjectWrap::Unwrap<Context>(args.This());
}


void Context::Close() {
    if (context_ != NULL) {
        zmq_term(context_);
        context_ = NULL;
        Unref();
    }
}

Handle<Value> Context::Close(const Arguments& args) {
    HandleScope scope;
    GetContext(args)->Close();
    return Undefined();
}



/*
 * Socket methods.
 */

static Persistent<String> events_symbol;


void Socket::Initialize(v8::Handle<v8::Object> target) {
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

    NODE_DEFINE_CONSTANT(t, ZMQ_HWM);
    NODE_DEFINE_CONSTANT(t, ZMQ_SWAP);
    NODE_DEFINE_CONSTANT(t, ZMQ_AFFINITY);
    NODE_DEFINE_CONSTANT(t, ZMQ_IDENTITY);
    NODE_DEFINE_CONSTANT(t, ZMQ_SUBSCRIBE);
    NODE_DEFINE_CONSTANT(t, ZMQ_UNSUBSCRIBE);
    NODE_DEFINE_CONSTANT(t, ZMQ_RATE);
    NODE_DEFINE_CONSTANT(t, ZMQ_RECOVERY_IVL);
    NODE_DEFINE_CONSTANT(t, ZMQ_MCAST_LOOP);
    NODE_DEFINE_CONSTANT(t, ZMQ_SNDBUF);
    NODE_DEFINE_CONSTANT(t, ZMQ_RCVBUF);
    NODE_DEFINE_CONSTANT(t, ZMQ_RCVMORE);
    NODE_DEFINE_CONSTANT(t, ZMQ_FD);
    NODE_DEFINE_CONSTANT(t, ZMQ_EVENTS);
    NODE_DEFINE_CONSTANT(t, ZMQ_TYPE);
    NODE_DEFINE_CONSTANT(t, ZMQ_LINGER);
    NODE_DEFINE_CONSTANT(t, ZMQ_RECONNECT_IVL);
    NODE_DEFINE_CONSTANT(t, ZMQ_BACKLOG);

    NODE_DEFINE_CONSTANT(t, ZMQ_POLLIN);
    NODE_DEFINE_CONSTANT(t, ZMQ_POLLOUT);
    NODE_DEFINE_CONSTANT(t, ZMQ_POLLERR);

    NODE_DEFINE_CONSTANT(t, ZMQ_SNDMORE);
    NODE_DEFINE_CONSTANT(t, ZMQ_NOBLOCK);

    NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(t, "getsockopt", GetSockOpt);
    NODE_SET_PROTOTYPE_METHOD(t, "setsockopt", SetSockOpt);
    NODE_SET_PROTOTYPE_METHOD(t, "_recv", Recv);
    NODE_SET_PROTOTYPE_METHOD(t, "_send", Send);
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

    events_symbol = NODE_PSYMBOL("events");

    target->Set(String::NewSymbol("Socket"), t->GetFunction());
}


Socket::~Socket() {
    Close();
}


Handle<Value> Socket::New(const Arguments &args) {
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

Socket::Socket(Context *context, int type) : EventEmitter () {
    socket_ = zmq_socket(context->context_, type);

    size_t zmq_fd_size = sizeof(int);
    int fd;
    zmq_getsockopt(socket_, ZMQ_FD, &fd, &zmq_fd_size);

    ev_init(&watcher_, Socket::Callback);
    ev_io_set(&watcher_, fd, EV_READ);
    watcher_.data = this;
    ev_io_start(EV_DEFAULT_UC_ &watcher_);
}

Socket* Socket::GetSocket(const Arguments &args) {
    return ObjectWrap::Unwrap<Socket>(args.This());
}


void Socket::Callback(EV_P_ ev_io *w, int ev_revents) {
    Socket *socket = static_cast<Socket*>(w->data);
    socket->Emit(events_symbol, 0, NULL);
}


template<typename T>
Handle<Value> Socket::GetSockOpt(int option) {
    T value = 0;
    size_t len = sizeof(T);
    if (zmq_getsockopt(socket_, option, &value, &len) < 0)
        return ThrowException(ExceptionFromError());
    return Integer::New(value);
}

template<typename T>
Handle<Value> Socket::SetSockOpt(int option, Handle<Value> wrappedValue) {
    if (!wrappedValue->IsNumber())
        return ThrowException(Exception::TypeError(
            String::New("Value must be an integer")));
    T value = (T) wrappedValue->ToInteger()->Value();
    if (zmq_setsockopt(socket_, option, &value, sizeof(T)) < 0)
        return ThrowException(ExceptionFromError());
    return Undefined();
}

template<> Handle<Value>
Socket::GetSockOpt<char*>(int option) {
    char value[1024];
    size_t len = sizeof(value) - 1;
    if (zmq_getsockopt(socket_, option, value, &len) < 0)
        return ThrowException(ExceptionFromError());
    value[len] = '\0';
    return v8::String::New(value);
}

template<> Handle<Value>
Socket::SetSockOpt<char*>(int option, Handle<Value> wrappedValue) {
    if (!wrappedValue->IsString())
        return ThrowException(Exception::TypeError(
            String::New("Value must be a string")));
    String::Utf8Value value(wrappedValue->ToString());
    if (zmq_setsockopt(socket_, option, *value, value.length()) < 0)
        return ThrowException(ExceptionFromError());
    return Undefined();
}

Handle<Value> Socket::GetSockOpt(const Arguments &args) {
    Socket *socket = GetSocket(args);
    if (args.Length() != 1)
        return ThrowException(Exception::Error(
            String::New("Must pass an option")));
    if (!args[0]->IsNumber())
        return ThrowException(Exception::TypeError(
            String::New("Option must be an integer")));
    int64_t option = args[0]->ToInteger()->Value();

    // FIXME: How to handle ZMQ_FD on Windows?
    switch (option) {
    case ZMQ_HWM:
    case ZMQ_AFFINITY:
    case ZMQ_SNDBUF:
    case ZMQ_RCVBUF:
    case ZMQ_RCVMORE:
        return socket->GetSockOpt<uint64_t>(option);
    case ZMQ_SWAP:
    case ZMQ_RATE:
    case ZMQ_RECOVERY_IVL:
    case ZMQ_MCAST_LOOP:
        return socket->GetSockOpt<int64_t>(option);
    case ZMQ_IDENTITY:
        return socket->GetSockOpt<char*>(option);
    case ZMQ_EVENTS:
        return socket->GetSockOpt<uint32_t>(option);
    case ZMQ_FD:
    case ZMQ_TYPE:
    case ZMQ_LINGER:
    case ZMQ_RECONNECT_IVL:
    case ZMQ_BACKLOG:
        return socket->GetSockOpt<int>(option);
    case ZMQ_SUBSCRIBE:
    case ZMQ_UNSUBSCRIBE:
        return ThrowException(Exception::TypeError(
            String::New("Socket option cannot be read")));
    default:
        return ThrowException(Exception::TypeError(
            String::New("Unknown socket option")));
    }
}

Handle<Value> Socket::SetSockOpt(const Arguments &args) {
    Socket *socket = GetSocket(args);
    if (args.Length() != 2)
        return ThrowException(Exception::Error(
            String::New("Must pass an option and a value")));
    if (!args[0]->IsNumber())
        return ThrowException(Exception::TypeError(
            String::New("Option must be an integer")));
    int64_t option = args[0]->ToInteger()->Value();

    switch (option) {
    case ZMQ_HWM:
    case ZMQ_AFFINITY:
    case ZMQ_SNDBUF:
    case ZMQ_RCVBUF:
        return socket->SetSockOpt<uint64_t>(option, args[1]);
    case ZMQ_SWAP:
    case ZMQ_RATE:
    case ZMQ_RECOVERY_IVL:
    case ZMQ_MCAST_LOOP:
        return socket->SetSockOpt<int64_t>(option, args[1]);
    case ZMQ_IDENTITY:
    case ZMQ_SUBSCRIBE:
    case ZMQ_UNSUBSCRIBE:
        return socket->SetSockOpt<char*>(option, args[1]);
    case ZMQ_LINGER:
    case ZMQ_RECONNECT_IVL:
    case ZMQ_BACKLOG:
        return socket->SetSockOpt<int>(option, args[1]);
    case ZMQ_RCVMORE:
    case ZMQ_EVENTS:
    case ZMQ_FD:
    case ZMQ_TYPE:
        return ThrowException(Exception::TypeError(
            String::New("Socket option cannot be written")));
    default:
        return ThrowException(Exception::TypeError(
            String::New("Unknown socket option")));
    }
}


struct Socket::BindState {
    BindState(Socket* sock_, Handle<Function> cb_, Handle<String> addr_)
            : addr(addr_) {
        sock_obj = Persistent<Object>::New(sock_->handle_);
        sock = sock_->socket_;
        cb = Persistent<Function>::New(cb_);
        error = 0;
    }

    ~BindState() {
        sock_obj.Dispose();
        cb.Dispose();
    }

    Persistent<Object> sock_obj;
    void* sock;
    Persistent<Function> cb;
    String::Utf8Value addr;
    int error;
};

Handle<Value> Socket::Bind(const Arguments &args) {
    HandleScope scope;
    if (!args[0]->IsString())
        return ThrowException(Exception::TypeError(
            String::New("Address must be a string!")));
    Local<String> addr = args[0]->ToString();
    if (args.Length() > 1 && !args[1]->IsFunction())
        return ThrowException(Exception::TypeError(
            String::New("Provided callback must be a function")));
    Local<Function> cb = Local<Function>::Cast(args[1]);

    BindState* state = new BindState(GetSocket(args), cb, addr);
    eio_custom(EIO_DoBind, EIO_PRI_DEFAULT, EIO_BindDone, state);
    ev_ref(EV_DEFAULT_UC);

    return Undefined();
}

int Socket::EIO_DoBind(eio_req *req) {
    BindState* state = (BindState*) req->data;
    if (zmq_bind(state->sock, *state->addr) < 0)
        state->error = zmq_errno();
    return 0;
}

int Socket::EIO_BindDone(eio_req *req) {
    BindState* state = (BindState*) req->data;
    HandleScope scope;

    TryCatch try_catch;
    Local<Value> argv[1];
    if (state->error)
        argv[0] = Exception::Error(String::New(zmq_strerror(state->error)));
    else
        argv[0] = Local<Value>::New(Undefined());
    state->cb->Call(v8::Context::GetCurrent()->Global(), 1, argv);
    if (try_catch.HasCaught())
        FatalException(try_catch);

    delete state;
    ev_unref(EV_DEFAULT_UC);
    return 0;
}


Handle<Value> Socket::Connect(const Arguments &args) {
    HandleScope scope;
    Socket *socket = GetSocket(args);
    if (!args[0]->IsString()) {
        return ThrowException(Exception::TypeError(
            String::New("Address must be a string!")));
    }

    String::Utf8Value address(args[0]->ToString());
    if (zmq_connect(socket->socket_, *address))
        return ThrowException(ExceptionFromError());
    return Undefined();
}


/*
 * An object that creates an empty ØMQ message, which can be used for
 * zmq_recv. After the receive call, a Buffer object wrapping the ØMQ
 * message can be requested. The reference for the ØMQ message will
 * remain while the data is in use by the Buffer.
 */

class Socket::IncomingMessage {
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

    inline Local<Value> GetBuffer() {
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

    Persistent<Object> buf_;
    MessageReference* msgref_;
};

Handle<Value> Socket::Recv(const Arguments &args) {
    HandleScope scope;

    int flags = 0;
    int argc = args.Length();
    if (argc == 1) {
        if (!args[0]->IsNumber())
            return ThrowException(Exception::TypeError(
                String::New("Argument should be an integer")));
        flags = args[0]->ToInteger()->Value();
    }
    else if (argc != 0)
        return ThrowException(Exception::TypeError(
            String::New("Only one argument at most was expected")));

    Socket *socket = GetSocket(args);
    IncomingMessage msg;
    if (zmq_recv(socket->socket_, msg, flags) < 0)
        return ThrowException(ExceptionFromError());
    return scope.Close(msg.GetBuffer());
}


/*
 * An object that creates a ØMQ message from the given Buffer Object,
 * and manages the reference to it using RAII. A persistent V8 handle
 * for the Buffer object will remain while its data is in use by ØMQ.
 */

class Socket::OutgoingMessage {
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

    zmq_msg_t msg_;
    BufferReference* bufref_;
};

Handle<Value> Socket::Send(const Arguments &args) {
    HandleScope scope;

    int argc = args.Length();
    if (argc != 1 && argc != 2)
        return ThrowException(Exception::TypeError(
            String::New("Must pass a Buffer and optionally flags")));
    if (!Buffer::HasInstance(args[0]))
          return ThrowException(Exception::TypeError(
              String::New("First argument should be a Buffer")));
    int flags = 0;
    if (argc == 2) {
        if (!args[1]->IsNumber())
            return ThrowException(Exception::TypeError(
                String::New("Second argument should be an integer")));
        flags = args[1]->ToInteger()->Value();
    }

    Socket *socket = GetSocket(args);
    OutgoingMessage msg(args[0]->ToObject());
    if (zmq_send(socket->socket_, msg, flags) < 0)
        return ThrowException(ExceptionFromError());
    return Undefined();
}


void Socket::Close() {
    if (socket_) {
        ev_io_stop(EV_DEFAULT_UC_ &watcher_);
        zmq_close(socket_);
        socket_ = NULL;
        Unref();
    }
}

Handle<Value> Socket::Close(const Arguments &args) {
    HandleScope scope;
    GetSocket(args)->Close();
    return Undefined();
}


} // namespace zmq



// --- E n t r y - p o i n t ---
extern "C" void
init(Handle<Object> target) {
    HandleScope scope;
    zmq::Context::Initialize(target);
    zmq::Socket::Initialize(target);
}
