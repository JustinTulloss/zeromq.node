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
#include <zmq_utils.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>
#include <set>
#include "nan.h"

#ifdef _WIN32
# include <delayimp.h>
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
#define ZMQ_CAN_SET_CTX (ZMQ_VERSION_MAJOR == 3 && ZMQ_VERSION_MINOR >= 2) || ZMQ_VERSION_MAJOR > 3

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

  class Context : public Nan::ObjectWrap {
    friend class Socket;
    public:
      static NAN_MODULE_INIT(Initialize);
      virtual ~Context();

    private:
      Context(int io_threads);
      static NAN_METHOD(New);
      static Context *GetContext(const Nan::FunctionCallbackInfo<Value>&);
      void Close();
      static NAN_METHOD(Close);
#if ZMQ_CAN_SET_CTX
      static NAN_METHOD(GetOpt);
      static NAN_METHOD(SetOpt);
#endif

      void* context_;
  };

  class Socket : public Nan::ObjectWrap {
    public:
      static NAN_MODULE_INIT(Initialize);
      virtual ~Socket();
      void NotifyReadReady();
      void NotifySendReady();
      void CallbackIfReady();

#if ZMQ_CAN_MONITOR
      void MonitorEvent(uint16_t event_id, int32_t event_value, char *endpoint);
      void MonitorError(const char *error_msg);
#endif

    private:
      static NAN_METHOD(New);
      Socket(Context *context, int type);

      static Socket* GetSocket(const Nan::FunctionCallbackInfo<Value>&);
      static NAN_GETTER(GetState);

      static NAN_GETTER(GetPending);
      static NAN_SETTER(SetPending);

      template<typename T>
      Local<Value> GetSockOpt(int option);
      template<typename T>
      Local<Value> SetSockOpt(int option, Local<Value> wrappedValue);
      static NAN_METHOD(GetSockOpt);
      static NAN_METHOD(SetSockOpt);

      void _AttachToEventLoop();
      void _DetachFromEventLoop();
      static NAN_METHOD(AttachToEventLoop);
      static NAN_METHOD(DetachFromEventLoop);

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
      static NAN_METHOD(Readv);
      class OutgoingMessage;
      static NAN_METHOD(Send);
      static NAN_METHOD(Sendv);
      void Close();
      static NAN_METHOD(Close);

      Nan::Persistent<Object> context_;
      void *socket_;
      bool pending_;
      uint8_t state_;
      int32_t endpoints;
#if ZMQ_CAN_MONITOR
      void *monitor_socket_;
      uv_timer_t *monitor_handle_;
      int64_t timer_interval_;
      int64_t num_of_events_;
      static void UV_MonitorCallback(uv_timer_t* handle, int status);
      static NAN_METHOD(Monitor);
      void Unmonitor();
      static NAN_METHOD(Unmonitor);
#endif

      short PollForEvents();
      uv_poll_t *poll_handle_;
      static void UV_PollCallback(uv_poll_t* handle, int status, int events);
  };

  Nan::Persistent<String> send_callback_symbol;
  Nan::Persistent<String> read_callback_symbol;

#if ZMQ_CAN_MONITOR
  Nan::Persistent<String> monitor_symbol;
  Nan::Persistent<String> monitor_error;
  int monitors_count = 0;
#endif

  static NAN_MODULE_INIT(Initialize);

  /*
   * Helpers for dealing with ØMQ errors.
   */

  static inline const char*
  ErrorMessage() {
    return zmq_strerror(zmq_errno());
  }

  static inline Local<Value>
  ExceptionFromError() {
    return Nan::Error(ErrorMessage());
  }


  /*
   * Context methods.
   */

  NAN_MODULE_INIT(Context::Initialize) {
    Nan::HandleScope scope;
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(t, "close", Close);
#if ZMQ_CAN_SET_CTX
    Nan::SetPrototypeMethod(t, "setOpt", SetOpt);
    Nan::SetPrototypeMethod(t, "getOpt", GetOpt);
#endif

    Nan::Set(target, Nan::New("Context").ToLocalChecked(), Nan::GetFunction(t).ToLocalChecked());
  }


  Context::~Context() {
    Close();
  }

  NAN_METHOD(Context::New) {
    assert(info.IsConstructCall());
    int io_threads = 1;
    if (info.Length() == 1) {
      if (!info[0]->IsNumber()) {
        return Nan::ThrowTypeError("io_threads must be an integer");
      }
      io_threads = Nan::To<int>(info[0]).FromJust();
      if (io_threads < 1) {
        return Nan::ThrowRangeError("io_threads must be a positive number");
      }
    }
    Context *context = new Context(io_threads);
    context->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  Context::Context(int io_threads) : Nan::ObjectWrap() {
    context_ = zmq_init(io_threads);
    if (!context_) throw std::runtime_error(ErrorMessage());
  }

  Context *
  Context::GetContext(const Nan::FunctionCallbackInfo<Value>& info) {
    return Nan::ObjectWrap::Unwrap<Context>(info.This());
  }

  void
  Context::Close() {
    if (context_ != NULL) {
      if (zmq_term(context_) < 0) throw std::runtime_error(ErrorMessage());
      context_ = NULL;
    }
  }

  NAN_METHOD(Context::Close) {
    GetContext(info)->Close();
    return;
  }

#if ZMQ_CAN_SET_CTX
  NAN_METHOD(Context::SetOpt) {
    if (info.Length() != 2)
      return Nan::ThrowError("Must pass an option and a value");
    if (!info[0]->IsNumber() || !info[1]->IsNumber())
      return Nan::ThrowTypeError("Arguments must be numbers");
    int option = Nan::To<int32_t>(info[0]).FromJust();
    int value = Nan::To<int32_t>(info[1]).FromJust();

    Context *context = GetContext(info);
    if (zmq_ctx_set(context->context_, option, value) < 0)
      return Nan::ThrowError(ExceptionFromError());
    return;
  }

  NAN_METHOD(Context::GetOpt) {
    if (info.Length() != 1)
      return Nan::ThrowError("Must pass an option");
    if (!info[0]->IsNumber())
      return Nan::ThrowTypeError("Option must be an integer");
    int option = Nan::To<int32_t>(info[0]).FromJust();

    Context *context = GetContext(info);
    int value = zmq_ctx_get(context->context_, option);
    info.GetReturnValue().Set(Nan::New<Integer>(value));
  }
#endif
  /*
   * Socket methods.
   */

  NAN_MODULE_INIT(Socket::Initialize) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    Nan::SetAccessor(t->InstanceTemplate(),
      Nan::New("state").ToLocalChecked(), Socket::GetState);
    Nan::SetAccessor(t->InstanceTemplate(),
      Nan::New("pending").ToLocalChecked(), GetPending, SetPending);

    Nan::SetPrototypeMethod(t, "bind", Bind);
    Nan::SetPrototypeMethod(t, "bindSync", BindSync);
#if ZMQ_CAN_UNBIND
    Nan::SetPrototypeMethod(t, "unbind", Unbind);
    Nan::SetPrototypeMethod(t, "unbindSync", UnbindSync);
#endif
    Nan::SetPrototypeMethod(t, "connect", Connect);
    Nan::SetPrototypeMethod(t, "getsockopt", GetSockOpt);
    Nan::SetPrototypeMethod(t, "setsockopt", SetSockOpt);
    Nan::SetPrototypeMethod(t, "ref", AttachToEventLoop);
    Nan::SetPrototypeMethod(t, "unref", DetachFromEventLoop);
    Nan::SetPrototypeMethod(t, "recv", Recv);
    Nan::SetPrototypeMethod(t, "readv", Readv);
    Nan::SetPrototypeMethod(t, "send", Send);
    Nan::SetPrototypeMethod(t, "sendv", Sendv);
    Nan::SetPrototypeMethod(t, "close", Close);

#if ZMQ_CAN_DISCONNECT
    Nan::SetPrototypeMethod(t, "disconnect", Disconnect);
#endif

#if ZMQ_CAN_MONITOR
    Nan::SetPrototypeMethod(t, "monitor", Monitor);
    Nan::SetPrototypeMethod(t, "unmonitor", Unmonitor);
    monitor_symbol.Reset(Nan::New("onMonitorEvent").ToLocalChecked());
    monitor_error.Reset(Nan::New("onMonitorError").ToLocalChecked());
#endif

    Nan::Set(target, Nan::New("SocketBinding").ToLocalChecked(), Nan::GetFunction(t).ToLocalChecked());

    read_callback_symbol.Reset(Nan::New("onReadReady").ToLocalChecked());
    send_callback_symbol.Reset(Nan::New("onSendReady").ToLocalChecked());
  }

  Socket::~Socket() {
    Close();
  }

  NAN_METHOD(Socket::New) {
    assert(info.IsConstructCall());

    if (info.Length() != 2) {
      return Nan::ThrowError("Must pass a context and a type to constructor");
    }

    Context *context = Nan::ObjectWrap::Unwrap<Context>(info[0].As<Object>());

    if (!info[1]->IsNumber()) {
      return Nan::ThrowTypeError("Type must be an integer");
    }

    int type = Nan::To<int>(info[1]).FromJust();

    Socket *socket = new Socket(context, type);
    socket->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  short
  Socket::PollForEvents() {
    zmq_pollitem_t item = { socket_, 0, ZMQ_POLLIN, 0 };
    if (pending_)
      item.events |= ZMQ_POLLOUT;

    while (true) {
      int rc = zmq_poll(&item, 1, 0);
      if (rc < 0) {
        if (zmq_errno()==EINTR) {
          continue;
        }
        throw std::runtime_error(ErrorMessage());
      } else {
        break;
      }
    }
    return item.revents & item.events;
  }

  void
  Socket::NotifyReadReady() {
    Nan::HandleScope scope;
    Local<Value> callback_v = Nan::Get(this->handle(), Nan::New(read_callback_symbol)).ToLocalChecked();

    Nan::MakeCallback(this->handle(), callback_v.As<Function>(), 0, NULL);
  }

  void
  Socket::NotifySendReady() {
    Nan::HandleScope scope;
    Local<Value> callback_v = Nan::Get(this->handle(), Nan::New(send_callback_symbol)).ToLocalChecked();

    Nan::MakeCallback(this->handle(), callback_v.As<Function>(), 0, NULL);
  }

  void
  Socket::CallbackIfReady() {
    short events = PollForEvents();

    if ((events & ZMQ_POLLIN) != 0) {
      NotifyReadReady();
    }

    if ((events & ZMQ_POLLOUT) != 0) {
      NotifySendReady();
    }
  }

  void
  Socket::UV_PollCallback(uv_poll_t* handle, int status, int events) {
    if (status != 0) {
      Nan::ThrowError("I/O status: socket not ready !=0 ");
      return;
    }
    Socket* s = static_cast<Socket*>(handle->data);
    s->CallbackIfReady();
  }

#if ZMQ_CAN_MONITOR
  void
  Socket::MonitorEvent(uint16_t event_id, int32_t event_value, char *event_endpoint) {
    Nan::HandleScope scope;

    Local<Value> callback_v = Nan::Get(this->handle(), Nan::New(monitor_symbol)).ToLocalChecked();
    if (!callback_v->IsFunction()) {
      return;
    }

    Local<Value> argv[3];
    argv[0] = Nan::New<Integer>(event_id);
    argv[1] = Nan::New<Integer>(event_value);
    argv[2] = Nan::New<String>(event_endpoint).ToLocalChecked();

    Nan::MakeCallback(this->handle(), callback_v.As<Function>(), 3, argv);
  }

  void
  Socket::MonitorError(const char *error_msg) {
    Nan::HandleScope scope;

    Local<Value> callback_v = Nan::Get(this->handle(), Nan::New(monitor_error)).ToLocalChecked();
    if (!callback_v->IsFunction()) {
      return;
    }

    Local<Value> argv[1];
    argv[0] = Nan::New<String>(error_msg).ToLocalChecked();

    Nan::MakeCallback(this->handle(), callback_v.As<Function>(), 1, argv);
  }

  void
  Socket::UV_MonitorCallback(uv_timer_t* handle, int status) {
    Nan::HandleScope scope;
    Socket* s = static_cast<Socket*>(handle->data);
    zmq_msg_t msg1; /* 3.x has 1 message per event */

    zmq_pollitem_t item;
    item.socket = s->monitor_socket_;
    item.events = ZMQ_POLLIN;

    const char* error = NULL;
    int64_t ittr = 0;
    while ((s->num_of_events_ == 0 || s->num_of_events_ > ittr++) && zmq_poll(&item, 1, 0)) {
      zmq_msg_init (&msg1);
      if (zmq_recvmsg (s->monitor_socket_, &msg1, ZMQ_DONTWAIT) > 0) {
        char event_endpoint[1025];
        uint16_t event_id;
        int32_t event_value;

#if ZMQ_VERSION_MAJOR >= 4
        uint8_t *data = static_cast<uint8_t *>(zmq_msg_data(&msg1));
        event_id = *reinterpret_cast<uint16_t *>(data);
        event_value = *reinterpret_cast<uint32_t *>(data + 2);

        zmq_msg_t msg2; /* 4.x has 2 messages per event */

        // get our next frame it may have the target address and safely copy to our buffer
        zmq_msg_init (&msg2);
        if (zmq_msg_more(&msg1) == 0 || zmq_recvmsg (s->monitor_socket_, &msg2, 0) == -1) {
          error = ErrorMessage();
          zmq_msg_close(&msg2);
          break;
        }

        // protect from overflow
        size_t len = zmq_msg_size(&msg2);
        // MIN message size and buffer size with null padding
        len = len < sizeof(event_endpoint)-1 ? len : sizeof(event_endpoint)-1;
        memcpy(event_endpoint, zmq_msg_data(&msg2), len);
        zmq_msg_close(&msg2);

        // null terminate our string
        event_endpoint[len]=0;
#else
        // monitoring on zmq < 4 used zmq_event_t
        zmq_event_t event;
        memcpy (&event, zmq_msg_data (&msg1), sizeof (zmq_event_t));
        event_id = event.event;

        // Bit of a hack, but all events in the zmq_event_t union have the same layout so this will work for all event types.
        event_value = event.data.connected.fd;
        snprintf(event_endpoint, sizeof(event_endpoint), "%s", event.data.connected.addr);
#endif

        s->MonitorEvent(event_id, event_value, event_endpoint);
        zmq_msg_close(&msg1);
      }
      else {
        error = ErrorMessage();
        zmq_msg_close(&msg1);
        break;
      }
    }

    // If there was no error and we still monitor we reset the monitor timer
    if (error == NULL && s->monitor_handle_ != NULL) {
      uv_timer_start(s->monitor_handle_, reinterpret_cast<uv_timer_cb>(Socket::UV_MonitorCallback), s->timer_interval_, 0);
    }
    // If error raise the monitor error event and stop the monitor
    else if (error != NULL) {
      s->Unmonitor();
      s->MonitorError(error);
    }
  }
#endif

  Socket::Socket(Context *context, int type) : Nan::ObjectWrap() {
    context_.Reset(context->handle());
    socket_ = zmq_socket(context->context_, type);
    pending_ = false;
    state_ = STATE_READY;

    if (NULL == socket_) {
      Nan::ThrowError(ErrorMessage());
      return;
    }

    endpoints = 0;

    poll_handle_ = new uv_poll_t;

    poll_handle_->data = this;

    uv_os_sock_t socket;
    size_t len = sizeof(uv_os_sock_t);

    if (zmq_getsockopt(socket_, ZMQ_FD, &socket, &len)) {
      throw std::runtime_error(ErrorMessage());
    }

    #if ZMQ_CAN_MONITOR
      this->monitor_socket_ = NULL;
    #endif

    uv_poll_init_socket(uv_default_loop(), poll_handle_, socket);
    uv_poll_start(poll_handle_, UV_READABLE, Socket::UV_PollCallback);
  }

  Socket *
  Socket::GetSocket(const Nan::FunctionCallbackInfo<Value> &info) {
    return Nan::ObjectWrap::Unwrap<Socket>(info.This());
  }

  /*
   * This macro makes a call to GetSocket and checks the socket state. These two
   * things go hand in hand everywhere in our code.
   */
  #define GET_SOCKET(info)                                      \
      Socket* socket = GetSocket(info);                         \
      if (socket->state_ == STATE_CLOSED)                       \
          return Nan::ThrowTypeError("Socket is closed");       \
      if (socket->state_ == STATE_BUSY)                         \
          return Nan::ThrowTypeError("Socket is busy");

  NAN_GETTER(Socket::GetState) {
    Socket* socket = Nan::ObjectWrap::Unwrap<Socket>(info.Holder());
    info.GetReturnValue().Set(Nan::New<Integer>(socket->state_));
  }

  NAN_GETTER(Socket::GetPending) {
    Socket* socket = Nan::ObjectWrap::Unwrap<Socket>(info.Holder());
    info.GetReturnValue().Set(socket->pending_);
  }

  NAN_SETTER(Socket::SetPending) {
    if (!value->IsBoolean())
      return Nan::ThrowTypeError("Pending must be a boolean");

    Socket* socket = Nan::ObjectWrap::Unwrap<Socket>(info.Holder());
    socket->pending_ = Nan::To<bool>(value).FromJust();
  }

  template<typename T>
  Local<Value> Socket::GetSockOpt(int option) {
    T value = 0;
    size_t len = sizeof(T);
    while (true) {
      int rc = zmq_getsockopt(socket_, option, &value, &len);
      if (rc < 0) {
        if(zmq_errno()==EINTR) {
          continue;
        }
        Nan::ThrowError(ExceptionFromError());
        return Nan::Undefined();
      }
      break;
    }
    return Nan::New<Number>(value);
  }

  template<typename T>
  Local<Value> Socket::SetSockOpt(int option, Local<Value> wrappedValue) {
    if (!wrappedValue->IsNumber()) {
      Nan::ThrowError("Value must be an integer");
      return Nan::Undefined();
    }
    T value = Nan::To<T>(wrappedValue).FromJust();
    if (zmq_setsockopt(socket_, option, &value, sizeof(T)) < 0)
      Nan::ThrowError(ExceptionFromError());
    return Nan::Undefined();
  }

  template<> Local<Value>
  Socket::GetSockOpt<char*>(int option) {
    char value[1024];
    size_t len = sizeof(value) - 1;
    if (zmq_getsockopt(socket_, option, value, &len) < 0) {
      Nan::ThrowError(ExceptionFromError());
      return Nan::Undefined();
    }
    value[len] = '\0';
    return Nan::New<String>(value).ToLocalChecked();
  }

  template<> Local<Value>
  Socket::SetSockOpt<char*>(int option, Local<Value> wrappedValue) {
    if (!Buffer::HasInstance(wrappedValue)) {
      Nan::ThrowTypeError("Value must be a buffer");
      return Nan::Undefined();
    }
    Local<Object> buf = wrappedValue.As<Object>();
    size_t length = Buffer::Length(buf);
    if (zmq_setsockopt(socket_, option, Buffer::Data(buf), length) < 0)
      Nan::ThrowError(ExceptionFromError());
    return Nan::Undefined();
  }

  NAN_METHOD(Socket::GetSockOpt) {
    if (info.Length() != 1)
      return Nan::ThrowError("Must pass an option");
    if (!info[0]->IsNumber())
      return Nan::ThrowTypeError("Option must be an integer");
    int64_t option = Nan::To<int64_t>(info[0]).FromJust();

    GET_SOCKET(info);

    if (opts_int.count(option)) {
      info.GetReturnValue().Set(socket->GetSockOpt<int>(option));
    } else if (opts_uint32.count(option)) {
      info.GetReturnValue().Set(socket->GetSockOpt<uint32_t>(option));
    } else if (opts_int64.count(option)) {
      info.GetReturnValue().Set(socket->GetSockOpt<int64_t>(option));
    } else if (opts_uint64.count(option)) {
      info.GetReturnValue().Set(socket->GetSockOpt<uint64_t>(option));
    } else if (opts_binary.count(option)) {
      info.GetReturnValue().Set(socket->GetSockOpt<char*>(option));
    } else {
      return Nan::ThrowError(zmq_strerror(EINVAL));
    }
  }

  NAN_METHOD(Socket::SetSockOpt) {
    if (info.Length() != 2)
      return Nan::ThrowError("Must pass an option and a value");
    if (!info[0]->IsNumber())
      return Nan::ThrowTypeError("Option must be an integer");
    int64_t option = Nan::To<int64_t>(info[0]).FromJust();
    GET_SOCKET(info);

    if (opts_int.count(option)) {
      info.GetReturnValue().Set(socket->SetSockOpt<int>(option, info[1]));
    } else if (opts_uint32.count(option)) {
      info.GetReturnValue().Set(socket->SetSockOpt<uint32_t>(option, info[1]));
    } else if (opts_int64.count(option)) {
      info.GetReturnValue().Set(socket->SetSockOpt<int64_t>(option, info[1]));
    } else if (opts_uint64.count(option)) {
      info.GetReturnValue().Set(socket->SetSockOpt<int64_t>(option, info[1]));
    } else if (opts_binary.count(option)) {
      info.GetReturnValue().Set(socket->SetSockOpt<char*>(option, info[1]));
    } else {
      return Nan::ThrowError(zmq_strerror(EINVAL));
    }
  }

  void Socket::_AttachToEventLoop() {
    uv_ref(reinterpret_cast<uv_handle_t *>(this->poll_handle_));
  }

  NAN_METHOD(Socket::AttachToEventLoop) {
    GET_SOCKET(info);
    socket->_AttachToEventLoop();
  }

  void Socket::_DetachFromEventLoop() {
    uv_unref(reinterpret_cast<uv_handle_t *>(this->poll_handle_));
  }

  NAN_METHOD(Socket::DetachFromEventLoop) {
    GET_SOCKET(info);
    socket->_DetachFromEventLoop();
  }

  struct Socket::BindState {
    BindState(Socket* sock_, Local<Function> cb_, Local<String> addr_)
          : addr(addr_) {
      sock_obj.Reset(sock_->handle());
      sock = sock_->socket_;
      cb.Reset(cb_);
      error = 0;
    }

    ~BindState() {
      sock_obj.Reset();
      cb.Reset();
    }

    Nan::Persistent<Object> sock_obj;
    void* sock;
    Nan::Persistent<Function> cb;
    Nan::Utf8String addr;
    int error;
  };

  NAN_METHOD(Socket::Bind) {
    if (!info[0]->IsString())
      return Nan::ThrowTypeError("Address must be a string!");
    Local<String> addr = info[0].As<String>();
    if (info.Length() > 1 && !info[1]->IsFunction())
      return Nan::ThrowTypeError("Provided callback must be a function");
    Local<Function> cb = Local<Function>::Cast(info[1]);

    GET_SOCKET(info);

    BindState* state = new BindState(socket, cb, addr);
    uv_work_t* req = new uv_work_t;
    req->data = state;
    uv_queue_work(uv_default_loop(),
                  req,
                  UV_BindAsync,
                  (uv_after_work_cb)UV_BindAsyncAfter);
    socket->state_ = STATE_BUSY;

    return;
  }

  void Socket::UV_BindAsync(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    if (zmq_bind(state->sock, *state->addr) < 0)
        state->error = zmq_errno();
  }

  void Socket::UV_BindAsyncAfter(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    Nan::HandleScope scope;

    Local<Value> argv[1];

    if (state->error) {
      argv[0] = Nan::Error(zmq_strerror(state->error));
    } else {
      argv[0] = Nan::Undefined();
    }

    Local<Function> cb = Nan::New(state->cb);

    Socket *socket = Nan::ObjectWrap::Unwrap<Socket>(Nan::New(state->sock_obj));
    socket->state_ = STATE_READY;

    if (socket->endpoints == 0) {
      socket->Ref();
      socket->_AttachToEventLoop();
    }
    socket->endpoints += 1;

    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, 1, argv);

    delete state;
    delete req;
  }

  NAN_METHOD(Socket::BindSync) {
    if (!info[0]->IsString())
      return Nan::ThrowTypeError("Address must be a string!");
    Nan::Utf8String addr(info[0].As<String>());
    GET_SOCKET(info);
    socket->state_ = STATE_BUSY;
    if (zmq_bind(socket->socket_, *addr) < 0) {
      socket->state_ = STATE_READY;
      return Nan::ThrowError(ErrorMessage());
    }

    socket->state_ = STATE_READY;

    if (socket->endpoints == 0) {
      socket->Ref();
      socket->_AttachToEventLoop();
    }

    socket->endpoints += 1;

    return;
  }

#if ZMQ_CAN_UNBIND
  NAN_METHOD(Socket::Unbind) {
    if (!info[0]->IsString())
      return Nan::ThrowTypeError("Address must be a string!");
    Local<String> addr = info[0].As<String>();
    if (info.Length() > 1 && !info[1]->IsFunction())
      return Nan::ThrowTypeError("Provided callback must be a function");
    Local<Function> cb = Local<Function>::Cast(info[1]);

    GET_SOCKET(info);

    BindState* state = new BindState(socket, cb, addr);
    uv_work_t* req = new uv_work_t;
    req->data = state;
    uv_queue_work(uv_default_loop(),
                  req,
                  UV_UnbindAsync,
                  (uv_after_work_cb)UV_UnbindAsyncAfter);
    socket->state_ = STATE_BUSY;
    return;
  }

  void Socket::UV_UnbindAsync(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    if (zmq_unbind(state->sock, *state->addr) < 0)
        state->error = zmq_errno();
  }

  void Socket::UV_UnbindAsyncAfter(uv_work_t* req) {
    BindState* state = static_cast<BindState*>(req->data);
    Nan::HandleScope scope;

    Local<Value> argv[1];

    if (state->error) {
      argv[0] = Nan::Error(zmq_strerror(state->error));
    } else {
      argv[0] = Nan::Undefined();
    }

    Local<Function> cb = Nan::New(state->cb);

    Socket *socket = Nan::ObjectWrap::Unwrap<Socket>(Nan::New(state->sock_obj));
    socket->state_ = STATE_READY;

    if (--socket->endpoints == 0) {
      socket->Unref();
      socket->_DetachFromEventLoop();
    }

    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, 1, argv);

    delete state;
    delete req;
  }

  NAN_METHOD(Socket::UnbindSync) {
    if (!info[0]->IsString())
      return Nan::ThrowTypeError("Address must be a string!");
    Nan::Utf8String addr(info[0].As<String>());
    GET_SOCKET(info);
    socket->state_ = STATE_BUSY;
    if (zmq_unbind(socket->socket_, *addr) < 0) {
      socket->state_ = STATE_READY;
      return Nan::ThrowError(ErrorMessage());
    }

    socket->state_ = STATE_READY;

    if (--socket->endpoints == 0) {
      socket->Unref();
      socket->_DetachFromEventLoop();
    }

    return;
  }
#endif

  NAN_METHOD(Socket::Connect) {
    if (!info[0]->IsString()) {
      return Nan::ThrowTypeError("Address must be a string!");
    }

    GET_SOCKET(info);

    Nan::Utf8String address(info[0].As<String>());
    if (zmq_connect(socket->socket_, *address))
      return Nan::ThrowError(ErrorMessage());

    if (socket->endpoints++ == 0) {
      socket->Ref();
      socket->_AttachToEventLoop();
    }

    return;
  }

#if ZMQ_CAN_DISCONNECT
  NAN_METHOD(Socket::Disconnect) {

    if (!info[0]->IsString()) {
      return Nan::ThrowTypeError("Address must be a string!");
    }

    GET_SOCKET(info);

    Nan::Utf8String address(info[0].As<String>());
    if (zmq_disconnect(socket->socket_, *address))
      return Nan::ThrowError(ErrorMessage());
    if (--socket->endpoints == 0) {
      socket->Unref();
      socket->_DetachFromEventLoop();
    }

    return;
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
          buf_.Reset();
        }
      };

      inline operator zmq_msg_t*() {
        return *msgref_;
      }

      inline Local<Value> GetBuffer() {
        if (buf_.IsEmpty()) {
          Local<Object> buf_obj = Nan::NewBuffer((char*)zmq_msg_data(*msgref_), zmq_msg_size(*msgref_), FreeCallback, msgref_).ToLocalChecked();
          if (buf_obj.IsEmpty()) {
            return Local<Value>();
          }
          buf_.Reset(buf_obj);
        }
        return Nan::New(buf_);
      }

    private:
      static void FreeCallback(char* data, void* message) {
        delete static_cast<MessageReference*>(message);
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

      Nan::Persistent<Object> buf_;
      MessageReference* msgref_;
  };

#if ZMQ_CAN_MONITOR
  NAN_METHOD(Socket::Monitor) {
    int64_t timer_interval = 10; // default to 10ms interval
    int64_t num_of_events = 1; // default is 1 event per interval

    if (info.Length() > 0 && !info[0]->IsUndefined()) {
      if (!info[0]->IsNumber())
        return Nan::ThrowTypeError("Option must be an integer");
      timer_interval = Nan::To<int64_t>(info[0]).FromJust();
      if (timer_interval <= 0)
        return Nan::ThrowTypeError("Option must be a positive integer");
    }

    if (info.Length() > 1 && !info[1]->IsUndefined()) {
      if (!info[1]->IsNumber())
        return Nan::ThrowTypeError("numOfEvents must be an integer");
      num_of_events = Nan::To<int64_t>(info[1]).FromJust();
      if (num_of_events < 0)
        return Nan::ThrowTypeError("numOfEvents should be no less than zero");
    }

    GET_SOCKET(info);
    char addr[255];
    Context *context = Nan::ObjectWrap::Unwrap<Context>(Nan::New(socket->context_));
    sprintf(addr, "%s%d", "inproc://monitor.req.", monitors_count++);

    if(zmq_socket_monitor(socket->socket_, addr, ZMQ_EVENT_ALL) != -1) {
      socket->monitor_socket_ = zmq_socket (context->context_, ZMQ_PAIR);
      zmq_connect (socket->monitor_socket_, addr);
      socket->timer_interval_ = timer_interval;
      socket->num_of_events_ = num_of_events;
      socket->monitor_handle_ = new uv_timer_t;
      socket->monitor_handle_->data = socket;

      uv_timer_init(uv_default_loop(), socket->monitor_handle_);
      uv_timer_start(socket->monitor_handle_, reinterpret_cast<uv_timer_cb>(Socket::UV_MonitorCallback), timer_interval, 0);
    }

    return;
  }

  void
  Socket::Unmonitor() {
    // Make sure we are monitoring
    if (this->monitor_socket_ == NULL) {
      return;
    }

    // Passing NULL as addr will tell zmq to stop monitor
    zmq_socket_monitor(this->socket_, NULL, ZMQ_EVENT_ALL);

    // Close the monitor socket and stop timer
    if (zmq_close(this->monitor_socket_) < 0)
      throw std::runtime_error(ErrorMessage());
    uv_timer_stop(this->monitor_handle_);
    this->monitor_handle_ = NULL;
    this->monitor_socket_ = NULL;
  }

  NAN_METHOD(Socket::Unmonitor) {
    // We can't use the GET_SOCKET macro here as it requries the socket to be open,
    // which might not always be the case
    Socket* socket = GetSocket(info);
    socket->Unmonitor();
    return;
  }

#endif

  NAN_METHOD(Socket::Readv) {
    Socket* socket = GetSocket(info);
    if (socket->state_ != STATE_READY)
      return;

    int events;
    size_t events_size = sizeof(events);
    bool checkPollIn = true;

    int rc = 0;
    int flags = 0;
    int64_t more = 1;
    size_t more_size = sizeof(more);
    size_t index = 0;

    Local<Array> result = Nan::New<Array>();

    while (more == 1) {
      if (checkPollIn) {
        while (zmq_getsockopt(socket->socket_, ZMQ_EVENTS, &events, &events_size)) {
          if (zmq_errno() != EINTR)
            return Nan::ThrowError(ErrorMessage());
        }

        if ((events & ZMQ_POLLIN) == 0)
          return;
      }

      IncomingMessage part;

      while (true) {
        rc = zmq_msg_init(part);
        if (rc != 0) {
          if (zmq_errno()==EINTR) {
            continue;
          }
          return Nan::ThrowError(ErrorMessage());
        }
        break;
      }

      while (true) {
      #if ZMQ_VERSION_MAJOR == 2
        rc = zmq_recv(socket->socket_, part, flags);
      #elif ZMQ_VERSION_MAJOR == 3
        rc = zmq_recvmsg(socket->socket_, part, flags);
      #else
        rc = zmq_msg_recv(part, socket->socket_, flags);
        checkPollIn = false;
      #endif

        if (rc < 0) {
          if (zmq_errno() == EINTR)
            continue;
          return Nan::ThrowError(ErrorMessage());
        }

        Nan::Set(result, index++, part.GetBuffer());
        break;
      }

      while (zmq_getsockopt(socket->socket_, ZMQ_RCVMORE, &more, &more_size)) {
        if (zmq_errno() != EINTR)
          return Nan::ThrowError(ErrorMessage());
      }
    }

    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(Socket::Recv) {
    int flags = 0;
    int argc = info.Length();
    if (argc == 1) {
      if (!info[0]->IsNumber())
        return Nan::ThrowTypeError("Argument should be an integer");
      flags = Nan::To<int>(info[0]).FromJust();
    } else if (argc != 0) {
      return Nan::ThrowTypeError("Only one argument at most was expected");
    }

    GET_SOCKET(info);

    IncomingMessage msg;
    while (true) {
      int rc;
    #if ZMQ_VERSION_MAJOR == 2
      rc = zmq_recv(socket->socket_, msg, flags);
    #else
      rc = zmq_recvmsg(socket->socket_, msg, flags);
    #endif
      if (rc < 0) {
        if (zmq_errno()==EINTR) {
          continue;
        }
        return Nan::ThrowError(ErrorMessage());
      } else {
        break;
      }
    }
    info.GetReturnValue().Set(msg.GetBuffer());
  }

  /*
   * An object that creates a ØMQ message from the given Buffer Object,
   * and manages the reference to it using RAII. A persistent V8 handle
   * for the Buffer object will remain while its data is in use by ØMQ.
   */

  class Socket::OutgoingMessage {
    public:
      inline OutgoingMessage(Local<Object> buf) {
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
          inline BufferReference(Local<Object> buf) {
            loop = uv_default_loop();
            uv_async_init(loop, &async, reinterpret_cast<uv_async_cb>(cleanup));
            async.data = this;
            persistent.Reset(buf);
          }

          inline ~BufferReference() {
            persistent.Reset();
          }

          // Called by zmq when the message has been sent.
          // NOTE: May be called from a worker thread. Do not modify V8/Node.
          static void FreeCallback(void* data, void* message) {
            uv_async_send(&static_cast<BufferReference *>(message)->async);
          }

          static void cleanup(uv_async_t *handle, int status) {
            delete static_cast<BufferReference *>(handle->data);
          }
        private:
          Nan::Persistent<Object> persistent;
          uv_async_t async;
          uv_loop_t *loop;
      };

    zmq_msg_t msg_;
    BufferReference* bufref_;
  };

  NAN_METHOD(Socket::Sendv) {
    Socket* socket = GetSocket(info);
    if (socket->state_ != STATE_READY)
      return info.GetReturnValue().Set(false);

    int events;
    size_t events_size = sizeof(events);
    bool checkPollOut = true;
    bool readsReady = false;

    int rc;

    Local<Array> batch = info[0].As<Array>();
    size_t len = batch->Length();

    if (len == 0)
      return info.GetReturnValue().Set(true);

    if (len % 2 != 0)
      return Nan::ThrowTypeError("Batch length must be even!");

    for (size_t i = 0; i < len; i += 2) {
      if (checkPollOut) {
        while (zmq_getsockopt(socket->socket_, ZMQ_EVENTS, &events, &events_size)) {
          if (zmq_errno() != EINTR)
            return Nan::ThrowError(ErrorMessage());
        }

        if ((events & ZMQ_POLLIN) != 0) {
          readsReady = true;
        }

        if ((events & ZMQ_POLLOUT) == 0) {
          if (readsReady) {
            socket->NotifyReadReady();
          }
          return info.GetReturnValue().Set(false);
        }
      }

      Local<Object> buf = batch->Get(i).As<Object>();
      Local<Number> flagsObj = batch->Get(i + 1).As<Number>();

      int flags = Nan::To<int>(flagsObj).FromJust();
      size_t len = Buffer::Length(buf);

      zmq_msg_t msg;
      rc = zmq_msg_init_size(&msg, len);
      if (rc != 0)
        return Nan::ThrowError(ErrorMessage());

      char * cp = static_cast<char *>(zmq_msg_data(&msg));
      const char * dat = Buffer::Data(buf);
      std::copy(dat, dat + len, cp);

      while (true) {
        int rc;
      #if ZMQ_VERSION_MAJOR == 2
        rc = zmq_send(socket->socket_, &msg, flags);
      #elif ZMQ_VERSION_MAJOR == 3
        rc = zmq_sendmsg(socket->socket_, &msg, flags);
      #else
        rc = zmq_msg_send(&msg, socket->socket_, flags);
        checkPollOut = false;
      #endif
        if (rc < 0){
          if (zmq_errno() == EINTR) {
            continue;
          }
          return Nan::ThrowError(ErrorMessage());
        }
        break;
      }
    }

    while (zmq_getsockopt(socket->socket_, ZMQ_EVENTS, &events, &events_size)) {
      if (zmq_errno() != EINTR)
        return Nan::ThrowError(ErrorMessage());
    }

    if ((events & ZMQ_POLLIN) != 0) {
      readsReady = true;
    }

    if (readsReady) {
      socket->NotifyReadReady();
    }

    return info.GetReturnValue().Set(true);
  }

  // WARNING: the buffer passed here will be kept alive
  // until zmq_send completes, possibly on another thread.
  // Do not modify or reuse any buffer passed to send.
  // This is bad, but allows us to send without copying.
  NAN_METHOD(Socket::Send) {

    int argc = info.Length();
    if (argc != 1 && argc != 2)
      return Nan::ThrowTypeError("Must pass a Buffer and optionally flags");
    if (!Buffer::HasInstance(info[0]))
      return Nan::ThrowTypeError("First argument should be a Buffer");
    int flags = 0;
    if (argc == 2) {
      if (!info[1]->IsNumber())
        return Nan::ThrowTypeError("Second argument should be an integer");
      flags = Nan::To<int>(info[1]).FromJust();
    }

    GET_SOCKET(info);

#if 0  // zero-copy version, but doesn't properly pin buffer and so has GC issues
    OutgoingMessage msg(info[0].As<Object>());
    if (zmq_send(socket->socket_, msg, flags) < 0)
        return Nan::ThrowError(ErrorMessage());
#else // copying version that has no GC issues
    zmq_msg_t msg;
    Local<Object> buf = info[0].As<Object>();
    size_t len = Buffer::Length(buf);
    int res = zmq_msg_init_size(&msg, len);
    if (res != 0)
      return Nan::ThrowError(ErrorMessage());

    char * cp = static_cast<char *>(zmq_msg_data(&msg));
    const char * dat = Buffer::Data(buf);
    std::copy(dat, dat + len, cp);
    while (true) {
      int rc;
    #if ZMQ_VERSION_MAJOR == 2
      rc = zmq_send(socket->socket_, &msg, flags);
    #elif ZMQ_VERSION_MAJOR == 3
      rc = zmq_sendmsg(socket->socket_, &msg, flags);
    #else
      rc = zmq_msg_send(&msg, socket->socket_, flags);
    #endif
      if (rc < 0){
        if (zmq_errno()==EINTR) {
          continue;
        }
        return Nan::ThrowError(ErrorMessage());
      } else {
        break;
      }
    }
#endif // zero copy / copying version

    return;
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
      context_.Reset();

      if (this->endpoints > 0)
        this->Unref();
      this->endpoints = 0;

      uv_poll_stop(poll_handle_);
      uv_close(reinterpret_cast<uv_handle_t*>(poll_handle_), on_uv_close);
    }
  }

  NAN_METHOD(Socket::Close) {
    GET_SOCKET(info);
    socket->Close();
    return;
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
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);

    char version_info[16];
    snprintf(version_info, 16, "%d.%d.%d", major, minor, patch);

    info.GetReturnValue().Set(Nan::New<String>(version_info).ToLocalChecked());
  }

#if ZMQ_VERSION_MAJOR >= 4
   static NAN_METHOD(ZmqCurveKeypair) {

    char public_key [41];
    char secret_key [41];

    int rc = zmq_curve_keypair( public_key, secret_key);
    if (rc < 0) {
      return Nan::ThrowError("zmq_curve_keypair operation failed. Method support in libzmq v4+ -with-libsodium.");
    }

    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("public").ToLocalChecked(), Nan::New<String>(public_key).ToLocalChecked());
    Nan::Set(obj, Nan::New<String>("secret").ToLocalChecked(), Nan::New<String>(secret_key).ToLocalChecked());

    info.GetReturnValue().Set(obj);
  }
#endif

  static NAN_MODULE_INIT(Initialize) {
    Nan::HandleScope scope;

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
    NODE_DEFINE_CONSTANT(target, ZMQ_CAN_SET_CTX);
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
    #if ZMQ_VERSION_MAJOR >= 4
    NODE_DEFINE_CONSTANT(target, ZMQ_STREAM);
    #endif

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

    Nan::SetMethod(target, "zmqVersion", ZmqVersion);
    #if ZMQ_VERSION_MAJOR >= 4
    Nan::SetMethod(target, "zmqCurveKeypair", ZmqCurveKeypair);
    #endif

    Context::Initialize(target);
    Socket::Initialize(target);
  }
} // namespace zmq


// module

extern "C" NAN_MODULE_INIT(init) {
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
          reinterpret_cast<SetDllDirectoryFunc>(GetProcAddress(kernel32_dll, "SetDllDirectoryW"));
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
      assert (!FAILED(__HrLoadAllImportsForDll("libzmq-v100-mt-4_0_4.dll")) &&
          "delayload error");
    }
  }
#endif
  zmq::Initialize(target);
}

NODE_MODULE(zmq, init)
