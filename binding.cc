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

  class Socket;

  /*
   * Manages a reference to a zmq_msg_t instance
   * Will return the zmq_msg_t instance by being deferenced
   */

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
        if (buf_.IsEmpty() && msgref_) {
          delete msgref_;
          msgref_ = NULL;
        } else
          buf_.Reset();
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

      Nan::Persistent<Object> buf_;
      MessageReference* msgref_;
  };

  class Context : public Nan::ObjectWrap {
    friend class Socket;
    public:
      static NAN_MODULE_INIT(Initialize);
      virtual ~Context();

    private:
      Context();
      static NAN_METHOD(New);
      static Context *GetContext(const Nan::FunctionCallbackInfo<Value>&);
      void Close();
      static NAN_METHOD(Close);
#if ZMQ_CAN_SET_CTX
      static NAN_METHOD(Get);
      static NAN_METHOD(Set);
#endif

      void* context_;
  };

  class Socket : public Nan::ObjectWrap {
    public:
      static NAN_MODULE_INIT(Initialize);
      virtual ~Socket();
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

      static NAN_METHOD(Read);
      static NAN_METHOD(Send);
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

  Nan::Persistent<String> callback_symbol;
#if ZMQ_CAN_MONITOR
  Nan::Persistent<String> monitor_symbol;
  Nan::Persistent<String> monitor_error;
  int monitors_count = 0;
#endif

  static NAN_MODULE_INIT(Initialize);

  /*
   * Context methods.
   */

  NAN_MODULE_INIT(Context::Initialize) {
    Nan::HandleScope scope;
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(t, "close", Close);
#if ZMQ_CAN_SET_CTX
    Nan::SetPrototypeMethod(t, "set", Set);
    Nan::SetPrototypeMethod(t, "get", Get);
#endif

    Nan::Set(target, Nan::New("Context").ToLocalChecked(), Nan::GetFunction(t).ToLocalChecked());
  }


  Context::~Context() {
    Close();
  }

  NAN_METHOD(Context::New) {
    assert(info.IsConstructCall());
    Context *context = new Context();
    context->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  Context::Context() : Nan::ObjectWrap() {
#if ZMQ_VERSION < 30200
      context_ = zmq_init(1);
#else
      context_ = zmq_ctx_new();
#endif

    if (!context_) throw std::runtime_error(ErrorMessage());
  }

  Context *
  Context::GetContext(const Nan::FunctionCallbackInfo<Value>& info) {
    return Nan::ObjectWrap::Unwrap<Context>(info.This());
  }

  void
  Context::Close() {
    if (context_ != NULL) {
      int rc;

      while (true) {
#if ZMQ_VERSION < 30200
        rc = zmq_term(context_);
#else
        rc = zmq_ctx_destroy(context_);
#endif

        if (rc < 0) {
          if (zmq_errno() == EINTR) {
            continue;
          }

          throw std::runtime_error(ErrorMessage());
        }
        break;
      }

      context_ = NULL;
    }
  }

  NAN_METHOD(Context::Close) {
    GetContext(info)->Close();
  }

#if ZMQ_CAN_SET_CTX
  NAN_METHOD(Context::Set) {
    if (info.Length() != 2)
      return Nan::ThrowError("Must pass an option and a value");
    if (!info[0]->IsNumber() || !info[1]->IsNumber())
      return Nan::ThrowTypeError("Arguments must be numbers");
    int option = Nan::To<int32_t>(info[0]).FromJust();
    int value = Nan::To<int32_t>(info[1]).FromJust();

    Context *context = GetContext(info);
    while (zmq_ctx_set(context->context_, option, value))
      if (zmq_errno() != EINTR)
        return Nan::ThrowError(ExceptionFromError());
    return;
  }

  NAN_METHOD(Context::Get) {
    if (info.Length() != 1)
      return Nan::ThrowError("Must pass an option");
    if (!info[0]->IsNumber())
      return Nan::ThrowTypeError("Option must be an integer");
    int option = Nan::To<int32_t>(info[0]).FromJust();

    Context *context = GetContext(info);

    int rc;
    do {
      rc = zmq_ctx_get(context->context_, option);
      if (rc < 0) {
        if (zmq_errno() != EINTR)
          return Nan::ThrowError(ExceptionFromError());
        continue;
      }
    } while (rc < 0);

    info.GetReturnValue().Set(Nan::New<Integer>(rc));
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
    Nan::SetPrototypeMethod(t, "read", Read);
    Nan::SetPrototypeMethod(t, "send", Send);
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

    callback_symbol.Reset(Nan::New("onReady").ToLocalChecked());
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
        if (zmq_errno() == EINTR) {
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
  Socket::CallbackIfReady() {
    short events = PollForEvents();
    if (events != 0) {
      Nan::HandleScope scope;

      Local<Value> callback_v = Nan::Get(this->handle(), Nan::New(callback_symbol)).ToLocalChecked();
      if (!callback_v->IsFunction()) {
        return;
      }

      Local<Value> argv[2];
      argv[0] = Nan::New<Boolean>((events & ZMQ_POLLIN) != 0);
      argv[1] = Nan::New<Boolean>((events & ZMQ_POLLOUT) != 0);

      Nan::MakeCallback(this->handle(), callback_v.As<Function>(), 2, argv);
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
        if (zmq_errno() == EINTR) {
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
  }

#if ZMQ_CAN_DISCONNECT
  NAN_METHOD(Socket::Disconnect) {
    if (!info[0]->IsString())
      return Nan::ThrowTypeError("Address must be a string!");

    GET_SOCKET(info);

    Nan::Utf8String address(info[0].As<String>());
    if (zmq_disconnect(socket->socket_, *address))
      return Nan::ThrowError(ErrorMessage());

    if (--socket->endpoints == 0) {
      socket->Unref();
      socket->_DetachFromEventLoop();
    }
  }
#endif

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

    if (zmq_socket_monitor(socket->socket_, addr, ZMQ_EVENT_ALL) != -1) {
      socket->monitor_socket_ = zmq_socket(context->context_, ZMQ_PAIR);
      zmq_connect (socket->monitor_socket_, addr);
      socket->timer_interval_ = timer_interval;
      socket->num_of_events_ = num_of_events;
      socket->monitor_handle_ = new uv_timer_t;
      socket->monitor_handle_->data = socket;

      uv_timer_init(uv_default_loop(), socket->monitor_handle_);
      uv_timer_start(socket->monitor_handle_, reinterpret_cast<uv_timer_cb>(Socket::UV_MonitorCallback), timer_interval, 0);
    }
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
  }
#endif

  NAN_METHOD(Socket::Read) {
    Socket* socket = GetSocket(info);
    if (socket->state_ != STATE_READY)
      return;

    int events;
    size_t events_size = sizeof(events);
    bool checkPollIn = true;

    int rc = 0;
    int flags = 0;

#if ZMQ_VERSION_MAJOR <= 2
    int64_t more = 1;
#else
    int more = 1;
#endif

    size_t more_size = sizeof(more);
    size_t index = 0;

    Local<Array> result = Nan::New<Array>();

    while (more == 1) {
      if (checkPollIn) {
        while (zmq_getsockopt(socket->socket_, ZMQ_EVENTS, &events, &events_size))
          if (zmq_errno() != EINTR)
            return Nan::ThrowError(ErrorMessage());

        if ((events & ZMQ_POLLIN) == 0)
          return;
      }

      IncomingMessage part;

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

  NAN_METHOD(Socket::Send) {
    Socket* socket = GetSocket(info);
    if (socket->state_ != STATE_READY)
      return info.GetReturnValue().Set(false);

    int events;
    size_t events_size = sizeof(events);
    bool checkPollOut = true;

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

        if ((events & ZMQ_POLLOUT) == 0)
          return info.GetReturnValue().Set(false);
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

    if (checkPollOut) {
      while (zmq_getsockopt(socket->socket_, ZMQ_EVENTS, &events, &events_size)) {
        if (zmq_errno() != EINTR)
          return Nan::ThrowError(ErrorMessage());
      }
    }

    return info.GetReturnValue().Set(true);
  }

  static void
  on_uv_close(uv_handle_t *handle) {
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
  }

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
    char public_key[41];
    char secret_key[41];

    int rc = zmq_curve_keypair(public_key, secret_key);
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

    // empty objects to hold (string -> mixed) values for constants

    Local<Object> sendFlags = Nan::New<Object>();
    Local<Object> types = Nan::New<Object>();
    Local<Object> options = Nan::New<Object>();
    Local<Object> ctxOptions = Nan::New<Object>();

    Nan::Set(target, Nan::New("sendFlags").ToLocalChecked(), sendFlags);
    Nan::Set(target, Nan::New("types").ToLocalChecked(), types);
    Nan::Set(target, Nan::New("options").ToLocalChecked(), options);
    Nan::Set(target, Nan::New("ctxOptions").ToLocalChecked(), ctxOptions);

    // Helper macros for storing and exposing constants

    #define OPT(type, name) \
      opts_ ## type.insert(name); \
      Nan::Set(options, Nan::New<String>(#name).ToLocalChecked(), Nan::New<Integer>(name));

    #define CTX_OPT(name) \
      Nan::Set(ctxOptions, Nan::New<String>(#name).ToLocalChecked(), Nan::New<Integer>(name));

    #define SENDFLAG(name) \
      Nan::Set(sendFlags, Nan::New<String>(#name).ToLocalChecked(), Nan::New<Integer>(name));

    #define TYPE(name) \
      Nan::Set(types, Nan::New<String>(#name).ToLocalChecked(), Nan::New<Integer>(name));

    // Message send flags

    // Message send flags since ZMQ 2.2
    // http://api.zeromq.org/2-2:zmq-send

    #ifdef ZMQ_NOBLOCK
      SENDFLAG(ZMQ_NOBLOCK);
    #endif

    #ifdef ZMQ_SNDMORE
      SENDFLAG(ZMQ_SNDMORE);
    #endif

    // Message send flags since ZMQ 3.0
    // http://api.zeromq.org/3-0:zmq-send

    #ifdef ZMQ_DONTWAIT
      SENDFLAG(ZMQ_DONTWAIT);
    #endif

    #ifdef ZMQ_SNDLABEL
      SENDFLAG(ZMQ_SNDLABEL);
    #endif


    // Socket types

    // Socket types since ZMQ 2.2:
    // http://api.zeromq.org/2-2:zmq-socket

    #ifdef ZMQ_REQ
      TYPE(ZMQ_REQ);
    #endif

    #ifdef ZMQ_REP
      TYPE(ZMQ_REP);
    #endif

    #ifdef ZMQ_DEALER
      TYPE(ZMQ_DEALER);
    #endif

    #ifdef ZMQ_ROUTER
      TYPE(ZMQ_ROUTER);
    #endif

    #ifdef ZMQ_PUB
      TYPE(ZMQ_PUB);
    #endif

    #ifdef ZMQ_SUB
      TYPE(ZMQ_SUB);
    #endif

    #ifdef ZMQ_PUSH
      TYPE(ZMQ_PUSH);
    #endif

    #ifdef ZMQ_PULL
      TYPE(ZMQ_PULL);
    #endif

    #ifdef ZMQ_PAIR
      TYPE(ZMQ_PAIR);
    #endif

    // Socket types since ZMQ 3.0:
    // http://api.zeromq.org/3-0:zmq-socket

    #ifdef ZMQ_XREQ
      TYPE(ZMQ_XREQ);
    #endif

    #ifdef ZMQ_XREP
      TYPE(ZMQ_XREP);
    #endif

    #ifdef ZMQ_XPUB
      TYPE(ZMQ_XPUB);
    #endif

    #ifdef ZMQ_XSUB
      TYPE(ZMQ_XSUB);
    #endif

    // Socket types since ZMQ 4.0:
    // http://api.zeromq.org/4-0:zmq-socket

    #ifdef ZMQ_STREAM
      TYPE(ZMQ_STREAM);
    #endif


    // Context options

    // Context options since ZMQ 3.2:
    // http://api.zeromq.org/3-2:zmq-ctx-set

    #ifdef ZMQ_IO_THREADS
      CTX_OPT(ZMQ_IO_THREADS);
    #endif

    #ifdef ZMQ_MAX_SOCKETS
      CTX_OPT(ZMQ_MAX_SOCKETS);
    #endif

    // Context options since ZMQ 4.0:
    // http://api.zeromq.org/4-0:zmq-ctx-set

    #ifdef ZMQ_THREAD_SCHED_POLICY
      CTX_OPT(ZMQ_THREAD_SCHED_POLICY);
    #endif

    #ifdef ZMQ_THREAD_PRIORITY
      CTX_OPT(ZMQ_THREAD_PRIORITY);
    #endif

    #ifdef ZMQ_IPV6
      CTX_OPT(ZMQ_IPV6);
    #endif


    // Socket options since ZMQ 2.2:
    // http://api.zeromq.org/2-2:zmq-setsockopt
    // http://api.zeromq.org/2-2:zmq-getsockopt

    #ifdef ZMQ_TYPE
      OPT(int, ZMQ_TYPE);
    #endif

    #ifdef ZMQ_RCVMORE
      #if ZMQ_VERSION_MAJOR <= 2
        OPT(int64, ZMQ_RCVMORE);
      #else
        OPT(int, ZMQ_RCVMORE);
      #endif
    #endif

    #ifdef ZMQ_HWM
      OPT(uint64, ZMQ_HWM);
    #endif

    #ifdef ZMQ_SWAP
      OPT(int64, ZMQ_SWAP);
    #endif

    #ifdef ZMQ_AFFINITY
      OPT(uint64, ZMQ_AFFINITY);
    #endif

    #ifdef ZMQ_IDENTITY
      OPT(binary, ZMQ_IDENTITY);
    #endif

    #ifdef ZMQ_SUBSCRIBE
      OPT(binary, ZMQ_SUBSCRIBE);
    #endif

    #ifdef ZMQ_UNSUBSCRIBE
      OPT(binary, ZMQ_UNSUBSCRIBE);
    #endif

    #ifdef ZMQ_RCVTIMEO
      OPT(int, ZMQ_RCVTIMEO);
    #endif

    #ifdef ZMQ_SNDTIMEO
      OPT(int, ZMQ_SNDTIMEO);
    #endif

    #ifdef ZMQ_RATE
      #if ZMQ_VERSION_MAJOR <= 2
        OPT(int64, ZMQ_RATE);
      #else
        OPT(int, ZMQ_RATE);
      #endif
    #endif

    #ifdef ZMQ_RECOVERY_IVL
      #if ZMQ_VERSION_MAJOR <= 2
        OPT(int64, ZMQ_RECOVERY_IVL);
      #else
        OPT(int, ZMQ_RECOVERY_IVL);
      #endif
    #endif

    #ifdef ZMQ_RECOVERY_IVL_MSEC
      OPT(int64, ZMQ_RECOVERY_IVL_MSEC);
    #endif

    #ifdef ZMQ_MCAST_LOOP
      OPT(int64, ZMQ_MCAST_LOOP);
    #endif

    #ifdef ZMQ_SNDBUF
      #if ZMQ_VERSION_MAJOR <= 2
        OPT(uint64, ZMQ_SNDBUF);
      #else
        OPT(int, ZMQ_SNDBUF);
      #endif
    #endif

    #ifdef ZMQ_RCVBUF
      #if ZMQ_VERSION_MAJOR <= 2
        OPT(uint64, ZMQ_RCVBUF);
      #else
        OPT(int, ZMQ_RCVBUF);
      #endif
    #endif

    #ifdef ZMQ_LINGER
      OPT(int, ZMQ_LINGER);
    #endif

    #ifdef ZMQ_RECONNECT_IVL
      OPT(int, ZMQ_RECONNECT_IVL);
    #endif

    #ifdef ZMQ_RECONNECT_IVL_MAX
      OPT(int, ZMQ_RECONNECT_IVL_MAX);
    #endif

    #ifdef ZMQ_BACKLOG
      OPT(int, ZMQ_BACKLOG);
    #endif

    #ifdef ZMQ_FD
      OPT(int, ZMQ_FD);
    #endif

    #ifdef ZMQ_EVENTS
      #if ZMQ_VERSION_MAJOR <= 2
        OPT(int, ZMQ_EVENTS);
      #else
        OPT(uint32, ZMQ_EVENTS);
      #endif
    #endif

    // Socket options since ZMQ 3.0:
    // http://api.zeromq.org/3-0:zmq-setsockopt
    // http://api.zeromq.org/3-0:zmq-getsockopt

    #ifdef ZMQ_RCVLABEL
      OPT(int, ZMQ_RCVLABEL);
    #endif

    // Socket options since ZMQ 3.2:
    // http://api.zeromq.org/3-2:zmq-setsockopt
    // http://api.zeromq.org/3-2:zmq-getsockopt

    #ifdef ZMQ_SNDHWM
      OPT(int, ZMQ_SNDHWM);
    #endif

    #ifdef ZMQ_RCVHWM
      OPT(int, ZMQ_RCVHWM);
    #endif

    #ifdef ZMQ_MAXMSGSIZE
      OPT(int64, ZMQ_MAXMSGSIZE);
    #endif

    #ifdef ZMQ_MULTICAST_HOPS
      OPT(int, ZMQ_MULTICAST_HOPS);
    #endif

    #ifdef ZMQ_IPV4ONLY
      OPT(int, ZMQ_IPV4ONLY);
    #endif

    #ifdef ZMQ_DELAY_ATTACH_ON_CONNECT
      OPT(int, ZMQ_DELAY_ATTACH_ON_CONNECT);
    #endif

    #ifdef ZMQ_LAST_ENDPOINT
      OPT(binary, ZMQ_LAST_ENDPOINT);
    #endif

    #ifdef ZMQ_ROUTER_MANDATORY
      OPT(int, ZMQ_ROUTER_MANDATORY);
    #endif

    #ifdef ZMQ_XPUB_VERBOSE
      OPT(int, ZMQ_XPUB_VERBOSE);
    #endif

    #ifdef ZMQ_TCP_KEEPALIVE
      OPT(int, ZMQ_TCP_KEEPALIVE);
    #endif

    #ifdef ZMQ_TCP_KEEPALIVE_IDLE
      OPT(int, ZMQ_TCP_KEEPALIVE_IDLE);
    #endif

    #ifdef ZMQ_TCP_KEEPALIVE_CNT
      OPT(int, ZMQ_TCP_KEEPALIVE_CNT);
    #endif

    #ifdef ZMQ_TCP_KEEPALIVE_INTVL
      OPT(int, ZMQ_TCP_KEEPALIVE_INTVL);
    #endif

    #ifdef ZMQ_TCP_ACCEPT_FILTER
      OPT(binary, ZMQ_TCP_ACCEPT_FILTER);
    #endif

    // Socket options since ZMQ 4.0:
    // http://api.zeromq.org/4-0:zmq-setsockopt
    // http://api.zeromq.org/4-0:zmq-getsockopt

    #ifdef ZMQ_IPV6
      OPT(int, ZMQ_IPV6);
    #endif

    #ifdef ZMQ_IMMEDIATE
      OPT(int, ZMQ_IMMEDIATE);
    #endif

    #ifdef ZMQ_MECHANISM
      OPT(int, ZMQ_MECHANISM);
    #endif

    #ifdef ZMQ_ROUTER_RAW
      OPT(int, ZMQ_ROUTER_RAW);
    #endif

    #ifdef ZMQ_PROBE_ROUTER
      OPT(int, ZMQ_PROBE_ROUTER);
    #endif

    #ifdef ZMQ_REQ_CORRELATE
      OPT(int, ZMQ_REQ_CORRELATE);
    #endif

    #ifdef ZMQ_REQ_RELAXED
      OPT(int, ZMQ_REQ_RELAXED);
    #endif

    #ifdef ZMQ_PLAIN_SERVER
      OPT(int, ZMQ_PLAIN_SERVER);
    #endif

    #ifdef ZMQ_PLAIN_USERNAME
      OPT(binary, ZMQ_PLAIN_USERNAME);
    #endif

    #ifdef ZMQ_PLAIN_PASSWORD
      OPT(binary, ZMQ_PLAIN_PASSWORD);
    #endif

    #ifdef ZMQ_CURVE_SERVER
      OPT(int, ZMQ_CURVE_SERVER);
    #endif

    #ifdef ZMQ_CURVE_PUBLICKEY
      OPT(binary, ZMQ_CURVE_PUBLICKEY);
    #endif

    #ifdef ZMQ_CURVE_SECRETKEY
      OPT(binary, ZMQ_CURVE_SECRETKEY);
    #endif

    #ifdef ZMQ_CURVE_SERVERKEY
      OPT(binary, ZMQ_CURVE_SERVERKEY);
    #endif

    #ifdef ZMQ_ZAP_DOMAIN
      OPT(binary, ZMQ_ZAP_DOMAIN);
    #endif

    #ifdef ZMQ_CONFLATE
      OPT(int, ZMQ_CONFLATE);
    #endif

    // Socket options since ZMQ 4.1:
    // http://api.zeromq.org/4-1:zmq-setsockopt
    // http://api.zeromq.org/4-1:zmq-getsockopt

    #ifdef ZMQ_CONNECT_RID
      OPT(binary, ZMQ_CONNECT_RID);
    #endif

    #ifdef ZMQ_GSSAPI_PLAINTEXT
      OPT(int, ZMQ_GSSAPI_PLAINTEXT);
    #endif

    #ifdef ZMQ_GSSAPI_PRINCIPAL
      OPT(binary, ZMQ_GSSAPI_PRINCIPAL);
    #endif

    #ifdef ZMQ_GSSAPI_SERVER
      OPT(int, ZMQ_GSSAPI_SERVER);
    #endif

    #ifdef ZMQ_GSSAPI_SERVICE_PRINCIPAL
      OPT(binary, ZMQ_GSSAPI_SERVICE_PRINCIPAL);
    #endif

    #ifdef ZMQ_HANDSHAKE_IVL
      OPT(int, ZMQ_HANDSHAKE_IVL);
    #endif

    #ifdef ZMQ_ROUTER_HANDOVER
      OPT(int, ZMQ_ROUTER_HANDOVER);
    #endif

    #ifdef ZMQ_TOS
      OPT(int, ZMQ_TOS);
    #endif

    // TODO: ZMQ_IPC_FILTER_GID
    // TODO: ZMQ_IPC_FILTER_PID
    // TODO: ZMQ_IPC_FILTER_UID

    // Undefine helpers

    #undef OPT
    #undef CTX_OPT
    #undef SENDFLAG
    #undef TYPE

    // Expose methods

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
