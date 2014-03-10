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
#include "nan.h"

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
#define ZMQ_CAN_UNBIND (ZMQ_VERSION_MAJOR == 3 && ZMQ_VERSION_MINOR >= 2) || ZMQ_VERSION_MAJOR > 3
#define ZMQ_CAN_MONITOR (ZMQ_VERSION > 30201)

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

  class Context : public ObjectWrap {
    friend class Socket;
    public:
      static void Initialize(v8::Handle<v8::Object> target);
      virtual ~Context();

    private:
      Context(int io_threads);
      static NAN_METHOD(New);
      static Context *GetContext(_NAN_METHOD_ARGS);
      void Close();
      static NAN_METHOD(Close);
      void* context_;
  };

  class Socket : public ObjectWrap {
    public:
      static void Initialize(v8::Handle<v8::Object> target);
      virtual ~Socket();
      void CallbackIfReady();
#if ZMQ_CAN_MONITOR
      void MonitorEvent(uint16_t event_id, int32_t event_value, char *endpoint);
#endif

    private:
      static NAN_METHOD(New);
      Socket(Context *context, int type);

      static Socket* GetSocket(_NAN_METHOD_ARGS);
      static NAN_GETTER(GetState);

      static NAN_GETTER(GetPending);
      static NAN_SETTER(SetPending);

      template<typename T>
      Handle<Value> GetSockOpt(int option);
      template<typename T>
      Handle<Value> SetSockOpt(int option, Handle<Value> wrappedValue);
      static NAN_METHOD(GetSockOpt);
      static NAN_METHOD(SetSockOpt);

      struct BindState;
      static NAN_METHOD(Bind);

      static void UV_BindAsync(uv_work_t* req);
      static void UV_BindAsyncAfter(uv_work_t* req);

      static NAN_METHOD(BindSync);
#if ZMQ_CAN_UNBIND
      static NAN_METHOD(Unbind);

      static void UV_UnbindAsync(uv_work_t* req);
      static void UV_UnbindAsyncAfter(uv_work_t* req);

      static NAN_METHOD(UnbindSync);
#endif
      static NAN_METHOD(Connect);
#if ZMQ_CAN_DISCONNECT
      static NAN_METHOD(Disconnect);
#endif

      class IncomingMessage;
      static NAN_METHOD(Recv);
      class OutgoingMessage;
      static NAN_METHOD(Send);
      void Close();
      static NAN_METHOD(Close);

      Persistent<Object> context_;
      void *socket_;
      int32_t pending_;
      uint8_t state_;
      int32_t endpoints;
#if ZMQ_CAN_MONITOR
      void *monitor_socket_;
      uv_idle_t *monitor_handle_;
      static void UV_MonitorCallback(uv_idle_t* handle, int status);
      static NAN_METHOD(Monitor);
      static NAN_METHOD(Unmonitor);
#endif

      bool IsReady();
      uv_poll_t *poll_handle_;
      static void UV_PollCallback(uv_poll_t* handle, int status, int events);
  };

  Persistent<String> callback_symbol;
#if ZMQ_CAN_MONITOR
  Persistent<String> monitor_symbol;
  int monitors_count = 0;
#endif

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
    NanScope();
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

    target->Set(String::NewSymbol("Context"), t->GetFunction());
  }


  Context::~Context() {
    Close();
  }

  NAN_METHOD(Context::New) {
    NanScope();
    assert(args.IsConstructCall());
    int io_threads = 1;
    if (args.Length() == 1) {
      if (!args[0]->IsNumber()) {
        return NanThrowTypeError("io_threads must be an integer");
      }
      io_threads = (int) args[0]->ToInteger()->Value();
      if (io_threads < 1) {
        return NanThrowRangeError("io_threads must be a positive number");
      }
    }
    Context *context = new Context(io_threads);
    context->Wrap(args.This());
    NanReturnValue(args.This());
  }

  Context::Context(int io_threads) : ObjectWrap() {
    context_ = zmq_init(io_threads);
    if (!context_) throw std::runtime_error(ErrorMessage());
  }

  Context *
  Context::GetContext(_NAN_METHOD_ARGS) {
    return ObjectWrap::Unwrap<Context>(args.This());
  }

  void
  Context::Close() {
    if (context_ != NULL) {
      if (zmq_term(context_) < 0) throw std::runtime_error(ErrorMessage());
      context_ = NULL;
    }
  }

  NAN_METHOD(Context::Close) {
    NanScope();
    GetContext(args)->Close();
    NanReturnUndefined();
  }

  /*
   * Socket methods.
   */

  void
  Socket::Initialize(v8::Handle<v8::Object> target) {
    NanScope();

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->InstanceTemplate()->SetAccessor(
      String::NewSymbol("state"), Socket::GetState);
    t->InstanceTemplate()->SetAccessor(
      String::NewSymbol("pending"), GetPending, SetPending);

    NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(t, "bindSync", BindSync);
#if ZMQ_CAN_UNBIND
    NODE_SET_PROTOTYPE_METHOD(t, "unbind", Unbind);
    NODE_SET_PROTOTYPE_METHOD(t, "unbindSync", UnbindSync);
#endif
    NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(t, "getsockopt", GetSockOpt);
    NODE_SET_PROTOTYPE_METHOD(t, "setsockopt", SetSockOpt);
    NODE_SET_PROTOTYPE_METHOD(t, "recv", Recv);
    NODE_SET_PROTOTYPE_METHOD(t, "send", Send);
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

#if ZMQ_CAN_DISCONNECT
    NODE_SET_PROTOTYPE_METHOD(t, "disconnect", Disconnect);
#endif

#if ZMQ_CAN_MONITOR
    NODE_SET_PROTOTYPE_METHOD(t, "monitor", Monitor);
    NODE_SET_PROTOTYPE_METHOD(t, "unmonitor", Unmonitor);
    NanAssignPersistent(String, monitor_symbol, String::NewSymbol("onMonitorEvent"));
#endif

    target->Set(String::NewSymbol("SocketBinding"), t->GetFunction());

    NanAssignPersistent(String, callback_symbol, String::NewSymbol("onReady"));
  }

  Socket::~Socket() {
    Close();
  }

  NAN_METHOD(Socket::New) {
    NanScope();
    assert(args.IsConstructCall());

    if (args.Length() != 2) {
      return NanThrowError("Must pass a context and a type to constructor");
    }

    Context *context = ObjectWrap::Unwrap<Context>(args[0]->ToObject());

    if (!args[1]->IsNumber()) {
      return NanThrowTypeError("Type must be an integer");
    }

    int type = (int) args[1]->ToInteger()->Value();

    Socket *socket = new Socket(context, type);
    socket->Wrap(args.This());
    NanReturnValue(args.This());
  }

  bool
  Socket::IsReady() {
    zmq_pollitem_t item = {socket_, 0, ZMQ_POLLIN, 0};
    if (pending_ > 0)
      item.events |= ZMQ_POLLOUT;

    int rc = zmq_poll(&item, 1, 0);
    if (rc < 0) {
      throw std::runtime_error(ErrorMessage());
    }
    return item.revents & item.events;
  }

  void
  Socket::CallbackIfReady() {
    if (this->IsReady()) {
      NanScope();

      Local<Value> callback_v = NanObjectWrapHandle(this)->Get(NanPersistentToLocal(callback_symbol));
      if (!callback_v->IsFunction()) {
        return;
      }

      TryCatch try_catch;

      MakeCallback(NanObjectWrapHandle(this), callback_v.As<Function>(), 0, NULL);

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

#if ZMQ_CAN_MONITOR
  void
  Socket::MonitorEvent(uint16_t event_id, int32_t event_value, char *event_endpoint) {
      NanScope();

      Local<Value> argv[3];
      argv[0] = NanNewLocal<Value>(Integer::New(event_id));
      argv[1] = NanNewLocal<Value>(Integer::New(event_value));
      argv[2] = NanNewLocal<Value>(String::New(event_endpoint));

      Local<Value> callback_v = NanObjectWrapHandle(this)->Get(NanPersistentToLocal(monitor_symbol));
      if (!callback_v->IsFunction()) {
        return;
      }

      TryCatch try_catch;

      MakeCallback(NanObjectWrapHandle(this), callback_v.As<Function>(), 3, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
  }

  void
  Socket::UV_MonitorCallback(uv_idle_t* handle, int status) {
    NanScope();
    Socket* s = static_cast<Socket*>(handle->data);
    zmq_msg_t msg1; /* 3.x has 1 message per event */

    zmq_pollitem_t item;
    item.socket = s->monitor_socket_;
    item.events = ZMQ_POLLIN;

    if (zmq_poll(&item, 1, 0)) {
      zmq_msg_init (&msg1);
      if (zmq_recvmsg (s->monitor_socket_, &msg1, ZMQ_DONTWAIT) > 0) {
        zmq_event_t event;
        char event_endpoint[1025];
        uint16_t event_id;
        int32_t event_value;
        memcpy (&event, zmq_msg_data (&msg1), sizeof (zmq_event_t));

        event_id = event.event;

#if ZMQ_VERSION_MAJOR >= 4
        zmq_msg_t msg2; /* 4.x has 2 messages per event */
        event_value = event.value;

        // get our next frame it may have the target address and safely copy to our buffer
        zmq_msg_init (&msg2);
        assert (zmq_msg_more(&msg1) != 0);
        assert (zmq_recvmsg (s->monitor_socket_, &msg2, 0) != -1);

        // protect from overflow
        size_t len = zmq_msg_size(&msg2);
        // MIN message size and buffer size with null padding
        len = len < sizeof(event_endpoint)-1 ? len : sizeof(event_endpoint)-1;
        memcpy(event_endpoint, zmq_msg_data(&msg2), len);

        // null terminate our string
        event_endpoint[len]=0;
#else
        // Bit of a hack, but all events in the zmq_event_t union have the same layout so this will work for all event types.
        event_value = event.data.connected.fd;
        snprintf(event_endpoint, sizeof(event_endpoint), "%s", event.data.connected.addr);
#endif

        s->MonitorEvent(event_id, event_value, event_endpoint);
      }
      else {
        uv_idle_stop(s->monitor_handle_);
      }

      zmq_msg_close (&msg1);
    }


  }
#endif

  Socket::Socket(Context *context, int type) : ObjectWrap() {
    NanAssignPersistent(Object, context_, NanObjectWrapHandle(context));
    socket_ = zmq_socket(context->context_, type);
    pending_ = 0;
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
  Socket::GetSocket(_NAN_METHOD_ARGS) {
    return ObjectWrap::Unwrap<Socket>(args.This());
  }

  /*
   * This macro makes a call to GetSocket and checks the socket state. These two
   * things go hand in hand everywhere in our code.
   */
  #define GET_SOCKET(args)                                      \
      Socket* socket = GetSocket(args);                         \
      if (socket->state_ == STATE_CLOSED)                       \
          return NanThrowTypeError("Socket is closed");         \
      if (socket->state_ == STATE_BUSY)                         \
          return NanThrowTypeError("Socket is busy");

  NAN_GETTER(Socket::GetState) {
    NanScope();
    Socket* socket = ObjectWrap::Unwrap<Socket>(args.Holder());
    NanReturnValue(Integer::New(socket->state_));
  }

  NAN_GETTER(Socket::GetPending) {
    NanScope();
    Socket* socket = ObjectWrap::Unwrap<Socket>(args.Holder());
    NanReturnValue(Integer::New(socket->pending_));
  }

  NAN_SETTER(Socket::SetPending) {
    NanScope();
    if (!value->IsNumber()) {
      NanThrowTypeError("Pending must be an integer");
    }

    Socket* socket = ObjectWrap::Unwrap<Socket>(args.Holder());
    socket->pending_ = value->Int32Value();
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

  NAN_METHOD(Socket::GetSockOpt) {
    NanScope();
    if (args.Length() != 1)
      return NanThrowError("Must pass an option");
    if (!args[0]->IsNumber())
      return NanThrowTypeError("Option must be an integer");
    int64_t option = args[0]->ToInteger()->Value();

    GET_SOCKET(args);

    if (opts_int.count(option)) {
      NanReturnValue(socket->GetSockOpt<int>(option));
    } else if (opts_uint32.count(option)) {
      NanReturnValue(socket->GetSockOpt<uint32_t>(option));
    } else if (opts_int64.count(option)) {
      NanReturnValue(socket->GetSockOpt<int64_t>(option));
    } else if (opts_uint64.count(option)) {
      NanReturnValue(socket->GetSockOpt<uint64_t>(option));
    } else if (opts_binary.count(option)) {
      NanReturnValue(socket->GetSockOpt<char*>(option));
    } else {
      return NanThrowError(zmq_strerror(EINVAL));
    }
  }

  NAN_METHOD(Socket::SetSockOpt) {
    NanScope();
    if (args.Length() != 2)
      return NanThrowError("Must pass an option and a value");
    if (!args[0]->IsNumber())
       return NanThrowTypeError("Option must be an integer");
    int64_t option = args[0]->ToInteger()->Value();
    GET_SOCKET(args);

    if (opts_int.count(option)) {
      NanReturnValue(socket->SetSockOpt<int>(option, args[1]));
    } else if (opts_uint32.count(option)) {
      NanReturnValue(socket->SetSockOpt<uint32_t>(option, args[1]));
    } else if (opts_int64.count(option)) {
      NanReturnValue(socket->SetSockOpt<int64_t>(option, args[1]));
    } else if (opts_uint64.count(option)) {
      NanReturnValue(socket->SetSockOpt<uint64_t>(option, args[1]));
    } else if (opts_binary.count(option)) {
      NanReturnValue(socket->SetSockOpt<char*>(option, args[1]));
    } else {
      return NanThrowError(zmq_strerror(EINVAL));
    }
  }

  struct Socket::BindState {
    BindState(Socket* sock_, Handle<Function> cb_, Handle<String> addr_)
          : addr(addr_) {
      NanAssignPersistent(Object, sock_obj, NanObjectWrapHandle(sock_));
      sock = sock_->socket_;
      NanAssignPersistent(Function, cb, cb_);
      error = 0;
    }

    ~BindState() {
      NanDisposePersistent(sock_obj);
      sock_obj.Clear();
      NanDisposePersistent(cb);
      cb.Clear();
    }

    Persistent<Object> sock_obj;
    void* sock;
    Persistent<Function> cb;
    String::Utf8Value addr;
    int error;
  };

  NAN_METHOD(Socket::Bind) {
    NanScope();
    if (!args[0]->IsString())
      return NanThrowTypeError("Address must be a string!");
    Local<String> addr = args[0].As<String>();
    if (args.Length() > 1 && !args[1]->IsFunction())
      return NanThrowTypeError("Provided callback must be a function");
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

    NanReturnUndefined();
  }

  void Socket::UV_BindAsync(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    if (zmq_bind(state->sock, *state->addr) < 0)
        state->error = zmq_errno();
  }

  void Socket::UV_BindAsyncAfter(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    NanScope();

    Local<Value> argv[1];

    if (state->error) {
      argv[0] = Exception::Error(String::New(zmq_strerror(state->error)));
    } else {
      argv[0] = NanNewLocal<Value>(Undefined());
    }

    Local<Function> cb = NanPersistentToLocal(state->cb);

    Socket *socket = ObjectWrap::Unwrap<Socket>(NanPersistentToLocal(state->sock_obj));
    socket->state_ = STATE_READY;

    if (socket->endpoints == 0)
      socket->Ref();
    socket->endpoints += 1;

    TryCatch try_catch;
    MakeCallback(v8::Context::GetCurrent()->Global(), cb, 1, argv);
    if (try_catch.HasCaught()) FatalException(try_catch);

    delete state;
    delete req;
  }

  NAN_METHOD(Socket::BindSync) {
    NanScope();
    if (!args[0]->IsString())
      return NanThrowTypeError("Address must be a string!");
    String::Utf8Value addr(args[0].As<String>());
    GET_SOCKET(args);
    socket->state_ = STATE_BUSY;
    if (zmq_bind(socket->socket_, *addr) < 0)
      return NanThrowError(ErrorMessage());

    socket->state_ = STATE_READY;

    if (socket->endpoints == 0)
      socket->Ref();

    socket->endpoints += 1;

    NanReturnUndefined();
  }

#if ZMQ_CAN_UNBIND
  NAN_METHOD(Socket::Unbind) {
    NanScope();
    if (!args[0]->IsString())
      return NanThrowTypeError("Address must be a string!");
    Local<String> addr = args[0].As<String>();
    if (args.Length() > 1 && !args[1]->IsFunction())
      return NanThrowTypeError("Provided callback must be a function");
    Local<Function> cb = Local<Function>::Cast(args[1]);

    GET_SOCKET(args);

    BindState* state = new BindState(socket, cb, addr);
    uv_work_t* req = new uv_work_t;
    req->data = state;
    uv_queue_work(uv_default_loop(),
                  req,
                  UV_UnbindAsync,
                  (uv_after_work_cb)UV_UnbindAsyncAfter);
    socket->state_ = STATE_BUSY;
    NanReturnUndefined();
  }

  void Socket::UV_UnbindAsync(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    if (zmq_unbind(state->sock, *state->addr) < 0)
        state->error = zmq_errno();
  }

  void Socket::UV_UnbindAsyncAfter(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    NanScope();

    Local<Value> argv[1];

    if (state->error) {
      argv[0] = Exception::Error(String::New(zmq_strerror(state->error)));
    } else {
      argv[0] = NanNewLocal<Value>(Undefined());
    }

    Local<Function> cb = NanPersistentToLocal(state->cb);

    Socket *socket = ObjectWrap::Unwrap<Socket>(NanPersistentToLocal(state->sock_obj));
    socket->state_ = STATE_READY;

    if (--socket->endpoints == 0)
      socket->Unref();

    TryCatch try_catch;
    MakeCallback(v8::Context::GetCurrent()->Global(), cb, 1, argv);
    if (try_catch.HasCaught()) FatalException(try_catch);

    delete state;
    delete req;
  }

  NAN_METHOD(Socket::UnbindSync) {
    NanScope();
    if (!args[0]->IsString())
      return NanThrowTypeError("Address must be a string!");
    String::Utf8Value addr(args[0].As<String>());
    GET_SOCKET(args);
    socket->state_ = STATE_BUSY;
    if (zmq_unbind(socket->socket_, *addr) < 0)
      return NanThrowError(ErrorMessage());

    socket->state_ = STATE_READY;

    if (--socket->endpoints == 0)
      socket->Unref();

    NanReturnUndefined();
  }
#endif

  NAN_METHOD(Socket::Connect) {
    NanScope();
    if (!args[0]->IsString()) {
      return NanThrowTypeError("Address must be a string!");
    }

    GET_SOCKET(args);

    String::Utf8Value address(args[0].As<String>());
    if (zmq_connect(socket->socket_, *address))
      return NanThrowError(ErrorMessage());

    if (socket->endpoints++ == 0)
      socket->Ref();

    NanReturnUndefined();
  }

#if ZMQ_CAN_DISCONNECT
  NAN_METHOD(Socket::Disconnect) {
    NanScope();

    if (!args[0]->IsString()) {
      return NanThrowTypeError("Address must be a string!");
    }

    GET_SOCKET(args);

    String::Utf8Value address(args[0].As<String>());
    if (zmq_disconnect(socket->socket_, *address))
      return NanThrowError(ErrorMessage());
    if (--socket->endpoints == 0)
      socket->Unref();

    NanReturnUndefined();
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
          NanDisposePersistent(buf_);
          buf_.Clear();
        }
      };

      inline operator zmq_msg_t*() {
        return *msgref_;
      }

      inline Local<Value> GetBuffer() {
        if (buf_.IsEmpty()) {
          Local<Object> buf_obj = NanNewBufferHandle((char*)zmq_msg_data(*msgref_), zmq_msg_size(*msgref_), FreeCallback, msgref_);
          if (buf_obj.IsEmpty()) {
            return Local<Value>();
          }
          NanAssignPersistent(Object, buf_, buf_obj);
        }
        return NanPersistentToLocal(buf_);
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

#if ZMQ_CAN_MONITOR
  NAN_METHOD(Socket::Monitor) {
    NanScope();
    GET_SOCKET(args);
    char addr[255];
    Context *context = ObjectWrap::Unwrap<Context>(NanPersistentToLocal(socket->context_));
    sprintf(addr, "%s%d", "inproc://monitor.req.", monitors_count++);

    if(zmq_socket_monitor(socket->socket_, addr, ZMQ_EVENT_ALL) != -1) {
      socket->monitor_socket_ = zmq_socket (context->context_, ZMQ_PAIR);
      zmq_connect (socket->monitor_socket_, addr);

      socket->monitor_handle_ = new uv_idle_t;

      socket->monitor_handle_->data = socket;

      uv_idle_init(uv_default_loop(), socket->monitor_handle_);
      uv_idle_start(socket->monitor_handle_, Socket::UV_MonitorCallback);
    }

    NanReturnUndefined();
  }

  NAN_METHOD(Socket::Unmonitor) {
    NanScope();
    Socket* socket = GetSocket(args);                         \
    if (zmq_close(socket->monitor_socket_) < 0)
      throw std::runtime_error(ErrorMessage());
    uv_idle_stop(socket->monitor_handle_);
    socket->monitor_handle_ = NULL;
    socket->monitor_socket_ = NULL;
    NanReturnUndefined();
  }
#endif

  NAN_METHOD(Socket::Recv) {
    NanScope();
    int flags = 0;
    int argc = args.Length();
    if (argc == 1) {
      if (!args[0]->IsNumber())
        return NanThrowTypeError("Argument should be an integer");
      flags = args[0]->ToInteger()->Value();
    } else if (argc != 0) {
      return NanThrowTypeError("Only one argument at most was expected");
    }

    GET_SOCKET(args);

    IncomingMessage msg;
    #if ZMQ_VERSION_MAJOR == 2
      if (zmq_recv(socket->socket_, msg, flags) < 0)
        return NanThrowError(ErrorMessage());
    #else
      if (zmq_recvmsg(socket->socket_, msg, flags) < 0)
        return NanThrowError(ErrorMessage());
    #endif
    NanReturnValue(msg.GetBuffer());
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
            NanAssignPersistent(Object, buf_, buf);
            NanMakeWeak(buf_, this, &WeakCheck);
          }

          inline ~BufferReference() {
            NanDisposePersistent(buf_);
            buf_.Clear();
          }

          // Called by zmq when the message has been sent.
          // NOTE: May be called from a worker thread. Do not modify V8/Node.
          static void FreeCallback(void* data, void* message) {
            // Raise a flag indicating that we're done with the buffer
            ((BufferReference*)message)->noLongerNeeded_ = true;
          }

         static NAN_WEAK_CALLBACK(BufferReference*, WeakCheck) {
           if (NAN_WEAK_CALLBACK_DATA(BufferReference*)->noLongerNeeded_) {
             delete NAN_WEAK_CALLBACK_DATA(BufferReference*);
           } else {
             // Still in use, revive, prevent GC
             NanMakeWeak(NAN_WEAK_CALLBACK_OBJECT, NAN_WEAK_CALLBACK_DATA(BufferReference*), &WeakCheck);
           }
         }

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
  NAN_METHOD(Socket::Send) {
    NanScope();

    int argc = args.Length();
    if (argc != 1 && argc != 2)
      return NanThrowTypeError("Must pass a Buffer and optionally flags");
    if (!Buffer::HasInstance(args[0]))
        return NanThrowTypeError("First argument should be a Buffer");
    int flags = 0;
    if (argc == 2) {
      if (!args[1]->IsNumber())
        return NanThrowTypeError("Second argument should be an integer");
      flags = args[1]->ToInteger()->Value();
    }

    GET_SOCKET(args);

#if 0  // zero-copy version, but doesn't properly pin buffer and so has GC issues
    OutgoingMessage msg(args[0].As<Object>());
    if (zmq_send(socket->socket_, msg, flags) < 0)
        return NanThrowError(ErrorMessage());
#else // copying version that has no GC issues
    zmq_msg_t msg;
    Local<Object> buf = args[0].As<Object>();
    size_t len = Buffer::Length(buf);
    int res = zmq_msg_init_size(&msg, len);
    if (res != 0)
      return NanThrowError(ErrorMessage());

    char * cp = (char *)zmq_msg_data(&msg);
    const char * dat = Buffer::Data(buf);
    std::copy(dat, dat + len, cp);

    #if ZMQ_VERSION_MAJOR == 2
      if (zmq_send(socket->socket_, &msg, flags) < 0)
        return NanThrowError(ErrorMessage());
    #elif ZMQ_VERSION_MAJOR == 3
      if (zmq_sendmsg(socket->socket_, &msg, flags) < 0)
        return NanThrowError(ErrorMessage());
    #else
      if (zmq_msg_send(&msg, socket->socket_, flags) < 0)
        return NanThrowError(ErrorMessage());
    #endif
#endif // zero copy / copying version

    NanReturnUndefined();
  }


  static void
  on_uv_close(uv_handle_t *handle)
  {
    delete handle;
  }

  void
  Socket::Close() {
    if (socket_) {
      if (zmq_close(socket_) < 0)
        throw std::runtime_error(ErrorMessage());
      socket_ = NULL;
      state_ = STATE_CLOSED;
      NanDisposePersistent(context_);
      context_.Clear();

      if (this->endpoints > 0)
        this->Unref();
      this->endpoints = 0;

      uv_poll_stop(poll_handle_);
      uv_close((uv_handle_t*)poll_handle_, on_uv_close);
    }
  }

  NAN_METHOD(Socket::Close) {
    NanScope();
    GET_SOCKET(args);
    socket->Close();
    NanReturnUndefined();
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

   static NAN_METHOD(ZmqVersion) {
    NanScope();
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);

    char version_info[16];
    snprintf(version_info, 16, "%d.%d.%d", major, minor, patch);

    NanReturnValue(String::New(version_info));
  }

  static void
  Initialize(Handle<Object> target) {
    NanScope();

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

    #if ZMQ_VERSION_MAJOR >= 4
    opts_int.insert(43); // ZMQ_MECHANISM
    opts_int.insert(44); // ZMQ_PLAIN_SERVER
    opts_binary.insert(45); // ZMQ_PLAIN_USERNAME
    opts_binary.insert(46); // ZMQ_PLAIN_PASSWORD
    opts_int.insert(47); // ZMQ_CURVE_SERVER
    opts_binary.insert(48); // ZMQ_CURVE_PUBLICKEY
    opts_binary.insert(49); // ZMQ_CURVE_SECRETKEY
    opts_binary.insert(50); // ZMQ_CURVE_SERVERKEY
    opts_binary.insert(55); // ZMQ_ZAP_DOMAIN
    #endif

    NODE_DEFINE_CONSTANT(target, ZMQ_CAN_DISCONNECT);
    NODE_DEFINE_CONSTANT(target, ZMQ_CAN_UNBIND);
    NODE_DEFINE_CONSTANT(target, ZMQ_CAN_MONITOR);
    NODE_DEFINE_CONSTANT(target, ZMQ_PUB);
    NODE_DEFINE_CONSTANT(target, ZMQ_SUB);
    #if ZMQ_VERSION_MAJOR >= 3
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
