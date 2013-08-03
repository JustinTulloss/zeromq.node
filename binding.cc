/*
 * Copyright (c) 2011 Justin Tulloss
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
#include <node_version.h>
#include <node_buffer.h>
#include <zmq.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>
#include <set>

#ifdef _WIN32
# define snprintf _snprintf_s
  typedef BOOL (WINAPI* SetDllDirectoryFunc)(wchar_t *lpPathName);
  class SetDllDirectoryCaller {
   public:
    explicit SetDllDirectoryCaller() : func_(NULL) { }
    ~SetDllDirectoryCaller() {
      if (func_)
        func_(NULL);
    }
    // Sets the SetDllDirectory function pointer to activates this object.
    void set_func(SetDllDirectoryFunc func) { func_ = func; }
   private:
    SetDllDirectoryFunc func_;
  };
#endif

#define ZMQ_CAN_DISCONNECT (ZMQ_VERSION_MAJOR == 3 && ZMQ_VERSION_MINOR >= 2) || ZMQ_VERSION_MAJOR > 3

using namespace v8;
using namespace node;

enum {
    STATE_READY
  , STATE_BUSY
  , STATE_CLOSED
};

namespace zmq {

  std::set<int> opts_int;
  std::set<int> opts_uint32;
  std::set<int> opts_int64;
  std::set<int> opts_uint64;
  std::set<int> opts_binary;

  class Socket;

  class Context : ObjectWrap {
    friend class Socket;
    public:
      static void Initialize(v8::Handle<v8::Object> target);
      virtual ~Context();

    private:
      Context(int io_threads);
      static Handle<Value> New(const Arguments& args);
      static Context* GetContext(const Arguments &args);

      void Close();
      static Handle<Value> Close(const Arguments& args);

      void* context_;
  };

  class Socket : ObjectWrap {
    public:
      static void Initialize(v8::Handle<v8::Object> target);
      virtual ~Socket();
      void CallbackIfReady();

    private:
      static Handle<Value> New(const Arguments &args);
      Socket(Context *context, int type);
      static Socket* GetSocket(const Arguments &args);

      static Handle<Value> GetState(Local<String> p, const AccessorInfo& info);

      template<typename T>
      Handle<Value> GetSockOpt(int option);
      template<typename T>
      Handle<Value> SetSockOpt(int option, Handle<Value> wrappedValue);
      static Handle<Value> GetSockOpt(const Arguments &args);
      static Handle<Value> SetSockOpt(const Arguments &args);

      struct BindState;
      static Handle<Value> Bind(const Arguments &args);

      static void UV_BindAsync(uv_work_t* req);
      static void UV_BindAsyncAfter(uv_work_t* req);

      static Handle<Value> BindSync(const Arguments &args);

      static Handle<Value> Connect(const Arguments &args);
      
#if ZMQ_CAN_DISCONNECT
      static Handle<Value> Disconnect(const Arguments &args);
#endif

      class IncomingMessage;
      static Handle<Value> Recv(const Arguments &args);

      class OutgoingMessage;
      static Handle<Value> Send(const Arguments &args);

      void Close();
      static Handle<Value> Close(const Arguments &args);

      Persistent<Object> context_;
      void *socket_;
      uint8_t state_;
      int32_t endpoints;

      bool IsReady();
      uv_poll_t *poll_handle_;
      static void UV_PollCallback(uv_poll_t* handle, int status, int events);
  };

  Persistent<String> callback_symbol;

  static void
  Initialize(Handle<Object> target);

  /*
   * Helpers for dealing with ØMQ errors.
   */

  static inline const char*
  ErrorMessage() {
    return zmq_strerror(zmq_errno());
  }

  static inline Local<Value>
  ExceptionFromError() {
    return Exception::Error(String::New(ErrorMessage()));
  }

  /*
   * Context methods.
   */

  void
  Context::Initialize(v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

    target->Set(String::NewSymbol("Context"), t->GetFunction());
  }


  Context::~Context() {
    Close();
  }

  Handle<Value>
  Context::New(const Arguments& args) {
    HandleScope scope;

    assert(args.IsConstructCall());

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

  Context::Context(int io_threads) : ObjectWrap() {
    context_ = zmq_init(io_threads);
    if (!context_) throw std::runtime_error(ErrorMessage());
  }

  Context *
  Context::GetContext(const Arguments &args) {
    return ObjectWrap::Unwrap<Context>(args.This());
  }


  void
  Context::Close() {
    if (context_ != NULL) {
      if (zmq_term(context_) < 0) throw std::runtime_error(ErrorMessage());
      context_ = NULL;
    }
  }

  Handle<Value>
  Context::Close(const Arguments& args) {
    HandleScope scope;
    GetContext(args)->Close();
    return Undefined();
  }

  /*
   * Socket methods.
   */

  void
  Socket::Initialize(v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->InstanceTemplate()->SetAccessor(
        String::NewSymbol("state"), GetState, NULL);

    NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(t, "bindSync", BindSync);
    NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(t, "getsockopt", GetSockOpt);
    NODE_SET_PROTOTYPE_METHOD(t, "setsockopt", SetSockOpt);
    NODE_SET_PROTOTYPE_METHOD(t, "recv", Recv);
    NODE_SET_PROTOTYPE_METHOD(t, "send", Send);
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);
    
#if ZMQ_CAN_DISCONNECT
    NODE_SET_PROTOTYPE_METHOD(t, "disconnect", Disconnect);
#endif

    target->Set(String::NewSymbol("Socket"), t->GetFunction());

    callback_symbol = NODE_PSYMBOL("onReady");
  }

  Socket::~Socket() {
    Close();
  }

  Handle<Value>
  Socket::New(const Arguments &args) {
    HandleScope scope;

    assert(args.IsConstructCall());

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

  bool
  Socket::IsReady() {
    zmq_pollitem_t item = {socket_, 0, ZMQ_POLLIN | ZMQ_POLLOUT, 0};
    int rc = zmq_poll(&item, 1, 0);
    if (rc < 0) {
      throw std::runtime_error(ErrorMessage());
    }
    return item.revents & (ZMQ_POLLIN | ZMQ_POLLOUT);
  }

  void
  Socket::CallbackIfReady() {
    if (this->IsReady()) {
      HandleScope scope;

      Local<Value> callback_v = this->handle_->Get(callback_symbol);
      if (!callback_v->IsFunction()) {
        return;
      }

      TryCatch try_catch;

      callback_v.As<Function>()->Call(this->handle_, 0, NULL);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
    }
  }

  void
  Socket::UV_PollCallback(uv_poll_t* handle, int status, int events) {
    assert(status == 0);

    Socket* s = static_cast<Socket*>(handle->data);
    s->CallbackIfReady();
  }

  Socket::Socket(Context *context, int type) : ObjectWrap() {
    #if NODE_VERSION_AT_LEAST(0, 11, 3)
      context_ = Persistent<Object>::New(Isolate::GetCurrent(), context->handle_);
    #else
      context_ = Persistent<Object>::New(context->handle_);
    #endif
    socket_ = zmq_socket(context->context_, type);
    state_ = STATE_READY;

    endpoints = 0;

    poll_handle_ = new uv_poll_t;

    poll_handle_->data = this;

    uv_os_sock_t socket;
    size_t len = sizeof(uv_os_sock_t);

    if (zmq_getsockopt(socket_, ZMQ_FD, &socket, &len)) {
      throw std::runtime_error(ErrorMessage());
    }

    uv_poll_init_socket(uv_default_loop(), poll_handle_, socket);
    uv_poll_start(poll_handle_, UV_READABLE, Socket::UV_PollCallback);
  }

  Socket *
  Socket::GetSocket(const Arguments &args) {
    return ObjectWrap::Unwrap<Socket>(args.This());
  }

  /*
   * This macro makes a call to GetSocket and checks the socket state. These two
   * things go hand in hand everywhere in our code.
   */
  #define GET_SOCKET(args)                              \
      Socket* socket = GetSocket(args);                 \
      if (socket->state_ == STATE_CLOSED)               \
          return ThrowException(Exception::TypeError(   \
              String::New("Socket is closed")));        \
      if (socket->state_ == STATE_BUSY)                 \
          return ThrowException(Exception::TypeError(   \
              String::New("Socket is busy")));

  Handle<Value>
  Socket::GetState(Local<String> p, const AccessorInfo& info) {
    Socket* socket = ObjectWrap::Unwrap<Socket>(info.Holder());
    return Integer::New(socket->state_);
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
    if (!Buffer::HasInstance(wrappedValue))
      return ThrowException(Exception::TypeError(
          String::New("Value must be a buffer")));
    Local<Object> buf = wrappedValue->ToObject();
    size_t length = Buffer::Length(buf);
    if (zmq_setsockopt(socket_, option, Buffer::Data(buf), length) < 0)
      return ThrowException(ExceptionFromError());
    return Undefined();
  }

  Handle<Value> Socket::GetSockOpt(const Arguments &args) {
    if (args.Length() != 1)
      return ThrowException(Exception::Error(
          String::New("Must pass an option")));
    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
          String::New("Option must be an integer")));
    int64_t option = args[0]->ToInteger()->Value();

    GET_SOCKET(args);

    if (opts_int.count(option)) {
      return socket->GetSockOpt<int>(option);
    } else if (opts_uint32.count(option)) {
      return socket->GetSockOpt<uint32_t>(option);
    } else if (opts_int64.count(option)) {
      return socket->GetSockOpt<int64_t>(option);
    } else if (opts_uint64.count(option)) {
      return socket->GetSockOpt<uint64_t>(option);
    } else if (opts_binary.count(option)) {
      return socket->GetSockOpt<char*>(option);
    } else {
      return ThrowException(Exception::Error(
        String::New(zmq_strerror(EINVAL))));
    }
  }

  Handle<Value> Socket::SetSockOpt(const Arguments &args) {
    if (args.Length() != 2)
      return ThrowException(Exception::Error(
        String::New("Must pass an option and a value")));
    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
          String::New("Option must be an integer")));
    int64_t option = args[0]->ToInteger()->Value();

    GET_SOCKET(args);

    if (opts_int.count(option)) {
      return socket->SetSockOpt<int>(option, args[1]);
    } else if (opts_uint32.count(option)) {
      return socket->SetSockOpt<uint32_t>(option, args[1]);
    } else if (opts_int64.count(option)) {
      return socket->SetSockOpt<int64_t>(option, args[1]);
    } else if (opts_uint64.count(option)) {
      return socket->SetSockOpt<uint64_t>(option, args[1]);
    } else if (opts_binary.count(option)) {
      return socket->SetSockOpt<char*>(option, args[1]);
    } else {
      return ThrowException(Exception::Error(
        String::New(zmq_strerror(EINVAL))));
    }
  }

  struct Socket::BindState {
    BindState(Socket* sock_, Handle<Function> cb_, Handle<String> addr_)
          : addr(addr_) {
      #if NODE_VERSION_AT_LEAST(0, 11, 3)
        sock_obj = Persistent<Object>::New(Isolate::GetCurrent(), sock_->handle_);
      #else
        sock_obj = Persistent<Object>::New(sock_->handle_);
      #endif
      sock = sock_->socket_;
      #if NODE_VERSION_AT_LEAST(0, 11, 3)
        cb = Persistent<Function>::New(Isolate::GetCurrent(), cb_);
      #else
        cb = Persistent<Function>::New(cb_);
      #endif
      error = 0;
    }

    ~BindState() {
      sock_obj.Dispose();
      sock_obj.Clear();
      cb.Dispose();
      cb.Clear();
    }

    Persistent<Object> sock_obj;
    void* sock;
    Persistent<Function> cb;
    String::Utf8Value addr;
    int error;
  };

  Handle<Value>
  Socket::Bind(const Arguments &args) {
    HandleScope scope;
    if (!args[0]->IsString())
      return ThrowException(Exception::TypeError(
          String::New("Address must be a string!")));
    Local<String> addr = args[0]->ToString();
    if (args.Length() > 1 && !args[1]->IsFunction())
      return ThrowException(Exception::TypeError(
        String::New("Provided callback must be a function")));
    Local<Function> cb = Local<Function>::Cast(args[1]);

    GET_SOCKET(args);

    BindState* state = new BindState(socket, cb, addr);
    uv_work_t* req = new uv_work_t;
    req->data = state;
    uv_queue_work(uv_default_loop(),
                  req,
                  UV_BindAsync,
                  (uv_after_work_cb)UV_BindAsyncAfter);
    socket->state_ = STATE_BUSY;

    return Undefined();
  }

  void Socket::UV_BindAsync(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    if (zmq_bind(state->sock, *state->addr) < 0)
        state->error = zmq_errno();
  }

  void Socket::UV_BindAsyncAfter(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    HandleScope scope;

    Local<Value> argv[1];
    if (state->error) argv[0] = Exception::Error(String::New(zmq_strerror(state->error)));
    else argv[0] = Local<Value>::New(Undefined());
    Local<Function> cb = Local<Function>::New(state->cb);

    Socket *socket = ObjectWrap::Unwrap<Socket>(state->sock_obj);
    socket->state_ = STATE_READY;
    delete state;

    if (socket->endpoints == 0)
      socket->Ref();
    socket->endpoints += 1;

    TryCatch try_catch;
    cb->Call(v8::Context::GetCurrent()->Global(), 1, argv);
    if (try_catch.HasCaught()) FatalException(try_catch);

    delete req;
  }

  Handle<Value>
  Socket::BindSync(const Arguments &args) {
    HandleScope scope;
    if (!args[0]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("Address must be a string!")));
    String::Utf8Value addr(args[0]->ToString());

    GET_SOCKET(args);

    socket->state_ = STATE_BUSY;

    if (zmq_bind(socket->socket_, *addr) < 0)
      return ThrowException(ExceptionFromError());

    socket->state_ = STATE_READY;

    if (socket->endpoints == 0)
      socket->Ref();
    socket->endpoints += 1;

    return Undefined();
  }

  Handle<Value>
  Socket::Connect(const Arguments &args) {
    HandleScope scope;
    if (!args[0]->IsString()) {
      return ThrowException(Exception::TypeError(
        String::New("Address must be a string!")));
    }

    GET_SOCKET(args);

    String::Utf8Value address(args[0]->ToString());
    if (zmq_connect(socket->socket_, *address))
      return ThrowException(ExceptionFromError());

    if (socket->endpoints == 0)
      socket->Ref();
    socket->endpoints += 1;

    return Undefined();
  }
  
#if ZMQ_CAN_DISCONNECT
  Handle<Value>
  Socket::Disconnect(const Arguments &args) {
    HandleScope scope;
    if (!args[0]->IsString()) {
      return ThrowException(Exception::TypeError(
        String::New("Address must be a string!")));
    }

    GET_SOCKET(args);

    String::Utf8Value address(args[0]->ToString());
    if (zmq_disconnect(socket->socket_, *address))
      return ThrowException(ExceptionFromError());

    socket->endpoints -= 1;
    if (socket->endpoints == 0)
      socket->Unref();

    return Undefined();
  }
#endif

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
        if (buf_.IsEmpty() && msgref_) {
          delete msgref_;
          msgref_ = NULL;
        } else {
          buf_.Dispose();
          buf_.Clear();
        }
      };

      inline operator zmq_msg_t*() {
        return *msgref_;
      }

      inline Local<Value> GetBuffer() {
        if (buf_.IsEmpty()) {
          #if NODE_VERSION_AT_LEAST(0, 11, 3)
            Local<Object> buf_obj = Buffer::New(
          #else
            Buffer* buf_obj = Buffer::New(
          #endif
            (char*)zmq_msg_data(*msgref_), zmq_msg_size(*msgref_),
            FreeCallback, msgref_);
          #if NODE_VERSION_AT_LEAST(0, 11, 3)
            if (buf_obj.IsEmpty())
          #else
            if (!buf_obj)
          #endif
            return Local<Value>();
          #if NODE_VERSION_AT_LEAST(0, 11, 3)
            buf_ = Persistent<Object>::New(Isolate::GetCurrent(), buf_obj);
          #else
            buf_ = Persistent<Object>::New(buf_obj->handle_);
          #endif
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

    GET_SOCKET(args);

    IncomingMessage msg;
    #if ZMQ_VERSION_MAJOR == 2
      if (zmq_recv(socket->socket_, msg, flags) < 0)
        return ThrowException(ExceptionFromError());
    #else
      if (zmq_recvmsg(socket->socket_, msg, flags) < 0)
        return ThrowException(ExceptionFromError());
    #endif
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
            // Keep the handle alive until zmq is done with the buffer
            noLongerNeeded_ = false;
            #if NODE_VERSION_AT_LEAST(0, 11, 3)
              buf_ = Persistent<Object>::New(Isolate::GetCurrent(), buf);
              buf_.MakeWeak(Isolate::GetCurrent(), this, &WeakCheck);
            #else
              buf_ = Persistent<Object>::New(buf);
              buf_.MakeWeak(this, &WeakCheck);
            #endif
          }

          inline ~BufferReference() {
            buf_.Dispose();
            buf_.Clear();
          }

          // Called by zmq when the message has been sent.
          // NOTE: May be called from a worker thread. Do not modify V8/Node.
          static void FreeCallback(void* data, void* message) {
            // Raise a flag indicating that we're done with the buffer
            ((BufferReference*)message)->noLongerNeeded_ = true;
          }

          // Called when V8 would like to GC buf_
          #if NODE_VERSION_AT_LEAST(0, 11, 3)
            static void WeakCheck(Isolate* isolate, Persistent<Object>* obj, BufferReference* data) {
              if ((data)->noLongerNeeded_) {
                delete data;
              } else {
                // Still in use, revive, prevent GC
                obj->MakeWeak(isolate, data, &WeakCheck);
              }
            }
          #else
            static void WeakCheck(v8::Persistent<v8::Value> obj, void* data) {
              if (((BufferReference*)data)->noLongerNeeded_) {
                delete (BufferReference*)data;
              } else {
                // Still in use, revive, prevent GC
                obj.MakeWeak(data, &WeakCheck);
              }
            }
          #endif

        private:
          bool noLongerNeeded_;
          Persistent<Object> buf_;
      };

    zmq_msg_t msg_;
    BufferReference* bufref_;
  };

  // WARNING: the buffer passed here will be kept alive
  // until zmq_send completes, possibly on another thread.
  // Do not modify or reuse any buffer passed to send.
  // This is bad, but allows us to send without copying.
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

    GET_SOCKET(args);

#if 0  // zero-copy version, but doesn't properly pin buffer and so has GC issues
    OutgoingMessage msg(args[0]->ToObject());
    if (zmq_send(socket->socket_, msg, flags) < 0)
        return ThrowException(ExceptionFromError());

#else // copying version that has no GC issues
    zmq_msg_t msg;
    Local<Object> buf = args[0]->ToObject();
    size_t len = Buffer::Length(buf);
    int res = zmq_msg_init_size(&msg, len);
    if (res != 0)
      return ThrowException(ExceptionFromError());

    char * cp = (char *)zmq_msg_data(&msg);
    const char * dat = Buffer::Data(buf);
    std::copy(dat, dat + len, cp);

    #if ZMQ_VERSION_MAJOR == 2
      if (zmq_send(socket->socket_, &msg, flags) < 0)
        return ThrowException(ExceptionFromError());
    #else
      if (zmq_sendmsg(socket->socket_, &msg, flags) < 0)
        return ThrowException(ExceptionFromError());
    #endif
#endif // zero copy / copying version

    return Undefined();
  }


  void
  Socket::Close() {
    if (socket_) {
      if (zmq_close(socket_) < 0)
        throw std::runtime_error(ErrorMessage());
      socket_ = NULL;
      state_ = STATE_CLOSED;
      context_.Dispose();
      context_.Clear();

      if (this->endpoints > 0)
        this->Unref();
      this->endpoints = 0;

      uv_poll_stop(poll_handle_);
    }
  }

  Handle<Value>
  Socket::Close(const Arguments &args) {
    HandleScope scope;
    GET_SOCKET(args);
    socket->Close();
    return Undefined();
  }

  // Make zeromq versions less than 2.1.3 work by defining
  // the new constants if they don't already exist
  #if (ZMQ_VERSION < 20103)
  #   define ZMQ_DEALER ZMQ_XREQ
  #   define ZMQ_ROUTER ZMQ_XREP
  #endif

  /*
   * Module functions.
   */

  static Handle<Value>
  ZmqVersion(const Arguments& args) {
    HandleScope scope;

    int major, minor, patch;
    zmq_version(&major, &minor, &patch);

    char version_info[16];
    snprintf(version_info, 16, "%d.%d.%d", major, minor, patch);

    return scope.Close(String::New(version_info));
  }

  static void
  Initialize(Handle<Object> target) {
    HandleScope scope;

    opts_int.insert(14); // ZMQ_FD
    opts_int.insert(16); // ZMQ_TYPE
    opts_int.insert(17); // ZMQ_LINGER
    opts_int.insert(18); // ZMQ_RECONNECT_IVL
    opts_int.insert(19); // ZMQ_BACKLOG
    opts_int.insert(21); // ZMQ_RECONNECT_IVL_MAX
    opts_int.insert(23); // ZMQ_SNDHWM
    opts_int.insert(24); // ZMQ_RCVHWM
    opts_int.insert(25); // ZMQ_MULTICAST_HOPS
    opts_int.insert(27); // ZMQ_RCVTIMEO
    opts_int.insert(28); // ZMQ_SNDTIMEO
    opts_int.insert(29); // ZMQ_RCVLABEL
    opts_int.insert(30); // ZMQ_RCVCMD
    opts_int.insert(31); // ZMQ_IPV4ONLY
    opts_int.insert(33); // ZMQ_ROUTER_MANDATORY
    opts_int.insert(34); // ZMQ_TCP_KEEPALIVE
    opts_int.insert(35); // ZMQ_TCP_KEEPALIVE_CNT
    opts_int.insert(36); // ZMQ_TCP_KEEPALIVE_IDLE
    opts_int.insert(37); // ZMQ_TCP_KEEPALIVE_INTVL
    opts_int.insert(39); // ZMQ_DELAY_ATTACH_ON_CONNECT
    opts_int.insert(40); // ZMQ_XPUB_VERBOSE
    opts_int.insert(41); // ZMQ_ROUTER_RAW
    opts_int.insert(42); // ZMQ_IPV6

    opts_int64.insert(3); // ZMQ_SWAP
    opts_int64.insert(8); // ZMQ_RATE
    opts_int64.insert(10); // ZMQ_MCAST_LOOP
    opts_int64.insert(20); // ZMQ_RECOVERY_IVL_MSEC
    opts_int64.insert(22); // ZMQ_MAXMSGSIZE

    opts_uint64.insert(1); // ZMQ_HWM
    opts_uint64.insert(4); // ZMQ_AFFINITY

    opts_binary.insert(5); // ZMQ_IDENTITY
    opts_binary.insert(6); // ZMQ_SUBSCRIBE
    opts_binary.insert(7); // ZMQ_UNSUBSCRIBE
    opts_binary.insert(32); // ZMQ_LAST_ENDPOINT
    opts_binary.insert(38); // ZMQ_TCP_ACCEPT_FILTER

    // transition types
    #if ZMQ_VERSION_MAJOR >= 3
    opts_int.insert(15); // ZMQ_EVENTS 3.x int
    opts_int.insert(8); // ZMQ_RATE 3.x int
    opts_int.insert(9); // ZMQ_RECOVERY_IVL 3.x int
    opts_int.insert(13); // ZMQ_RCVMORE 3.x int
    opts_int.insert(11); // ZMQ_SNDBUF 3.x int
    opts_int.insert(12); // ZMQ_RCVBUF 3.x int
    #else
    opts_uint32.insert(15); // ZMQ_EVENTS 2.x uint32_t
    opts_int64.insert(8); // ZMQ_RATE 2.x int64_t
    opts_int64.insert(9); // ZMQ_RECOVERY_IVL 2.x int64_t
    opts_int64.insert(13); // ZMQ_RCVMORE 2.x int64_t
    opts_uint64.insert(11); // ZMQ_SNDBUF 2.x uint64_t
    opts_uint64.insert(12); // ZMQ_RCVBUF 2.x uint64_t
    #endif

    NODE_DEFINE_CONSTANT(target, ZMQ_CAN_DISCONNECT);
    NODE_DEFINE_CONSTANT(target, ZMQ_PUB);
    NODE_DEFINE_CONSTANT(target, ZMQ_SUB);
    #if ZMQ_VERSION_MAJOR == 3
    NODE_DEFINE_CONSTANT(target, ZMQ_XPUB);
    NODE_DEFINE_CONSTANT(target, ZMQ_XSUB);
    #endif
    NODE_DEFINE_CONSTANT(target, ZMQ_REQ);
    NODE_DEFINE_CONSTANT(target, ZMQ_XREQ);
    NODE_DEFINE_CONSTANT(target, ZMQ_REP);
    NODE_DEFINE_CONSTANT(target, ZMQ_XREP);
    NODE_DEFINE_CONSTANT(target, ZMQ_DEALER);
    NODE_DEFINE_CONSTANT(target, ZMQ_ROUTER);
    NODE_DEFINE_CONSTANT(target, ZMQ_PUSH);
    NODE_DEFINE_CONSTANT(target, ZMQ_PULL);
    NODE_DEFINE_CONSTANT(target, ZMQ_PAIR);

    NODE_DEFINE_CONSTANT(target, ZMQ_POLLIN);
    NODE_DEFINE_CONSTANT(target, ZMQ_POLLOUT);
    NODE_DEFINE_CONSTANT(target, ZMQ_POLLERR);

    NODE_DEFINE_CONSTANT(target, ZMQ_SNDMORE);
    #if ZMQ_VERSION_MAJOR == 2
    NODE_DEFINE_CONSTANT(target, ZMQ_NOBLOCK);
    #endif

    NODE_DEFINE_CONSTANT(target, STATE_READY);
    NODE_DEFINE_CONSTANT(target, STATE_BUSY);
    NODE_DEFINE_CONSTANT(target, STATE_CLOSED);

    NODE_SET_METHOD(target, "zmqVersion", ZmqVersion);

    Context::Initialize(target);
    Socket::Initialize(target);
  }
} // namespace zmq


// module

extern "C" void
init(Handle<Object> target) {
#ifdef _MSC_VER
  // On Windows, inject the windows/lib folder into the DLL search path so that
  // it will pick up our bundled DLL in case we do not have zmq installed on
  // this system.
  HMODULE kernel32_dll = GetModuleHandleW(L"kernel32.dll");
  SetDllDirectoryCaller caller;
  SetDllDirectoryFunc set_dll_directory;
  wchar_t path[MAX_PATH] = L"";
  wchar_t pathDir[MAX_PATH] = L"";
  if (kernel32_dll != NULL) {
    set_dll_directory =
          (SetDllDirectoryFunc)GetProcAddress(kernel32_dll, "SetDllDirectoryW");
    if (set_dll_directory) {
      GetModuleFileNameW(GetModuleHandleW(L"zmq.node"), path, MAX_PATH - 1);
      wcsncpy(pathDir, path, wcsrchr(path, '\\') - path);
      path[0] = '\0';
      pathDir[wcslen(pathDir)] = '\0';
# ifdef _WIN64
      wcscat(pathDir, L"\\..\\..\\windows\\lib\\x64");
# else
      wcscat(pathDir, L"\\..\\..\\windows\\lib\\x86");
# endif
      _wfullpath(path, pathDir, MAX_PATH);
      set_dll_directory(path);
      caller.set_func(set_dll_directory);
      LoadLibrary("libzmq-v100-mt-3_2_2");
    }
  }
#endif
  zmq::Initialize(target);
}

NODE_MODULE(zmq, init)
