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

#define STATE_READY  0
#define STATE_BUSY   1
#define STATE_CLOSED 2

namespace zmq {
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
      static int EIO_DoBind(eio_req *req);
      static int EIO_BindDone(eio_req *req);
      static Handle<Value> BindSync(const Arguments &args);

      static Handle<Value> Connect(const Arguments &args);

      class IncomingMessage;
      static Handle<Value> Recv(const Arguments &args);

      class OutgoingMessage;
      static Handle<Value> Send(const Arguments &args);

      void Close();
      static Handle<Value> Close(const Arguments &args);

      Persistent<Object> context_;
      void *socket_;
      uint8_t state_;
  };

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

    target->Set(String::NewSymbol("Socket"), t->GetFunction());
  }


  Socket::~Socket() {
    Close();
  }


  Handle<Value>
  Socket::New(const Arguments &args) {
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

  Socket::Socket(Context *context, int type) : ObjectWrap() {
      context_ = Persistent<Object>::New(context->handle_);
      socket_ = zmq_socket(context->context_, type);
      state_ = STATE_READY;
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
    if (args.Length() != 2)
      return ThrowException(Exception::Error(
        String::New("Must pass an option and a value")));
    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
          String::New("Option must be an integer")));
    int64_t option = args[0]->ToInteger()->Value();

    GET_SOCKET(args);

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
    eio_custom(EIO_DoBind, EIO_PRI_DEFAULT, EIO_BindDone, state);
    ev_ref(EV_DEFAULT_UC);
    socket->state_ = STATE_BUSY;

    return Undefined();
  }

  int
  Socket::EIO_DoBind(eio_req *req) {
    BindState* state = (BindState*) req->data;
    if (zmq_bind(state->sock, *state->addr) < 0)
      state->error = zmq_errno();
    return 0;
  }

  int
  Socket::EIO_BindDone(eio_req *req) {
    BindState* state = (BindState*) req->data;
    HandleScope scope;

    Local<Value> argv[1];
    if (state->error) argv[0] = Exception::Error(String::New(zmq_strerror(state->error)));
    else argv[0] = Local<Value>::New(Undefined());
    Local<Function> cb = Local<Function>::New(state->cb);

    ObjectWrap::Unwrap<Socket>(state->sock_obj)->state_ = STATE_READY;
    delete state;

    TryCatch try_catch;
    cb->Call(v8::Context::GetCurrent()->Global(), 1, argv);
    if (try_catch.HasCaught()) FatalException(try_catch);

    ev_unref(EV_DEFAULT_UC);
    return 0;
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
          Buffer* buf_obj = Buffer::New(
            (char*)zmq_msg_data(*msgref_), zmq_msg_size(*msgref_),
            FreeCallback, msgref_);
          if (!buf_obj)
            return Local<Value>();
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

      GET_SOCKET(args);

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
              // Keep the handle alive until zmq is done with the buffer
              noLongerNeeded_ = false;
              buf_ = Persistent<Object>::New(buf);
              buf_.MakeWeak(this, &WeakCheck);
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
          static void WeakCheck(v8::Persistent<v8::Value> obj, void* data) {
              if (((BufferReference*)data)->noLongerNeeded_) {
                  delete (BufferReference*)data;
              } else {
                  // Still in use, revive, prevent GC
                  obj.MakeWeak(data, &WeakCheck);
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

      if (zmq_send(socket->socket_, &msg, flags) < 0)
          return ThrowException(ExceptionFromError());
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

  static void
  Initialize(Handle<Object> target) {
    HandleScope scope;

    NODE_DEFINE_CONSTANT(target, ZMQ_PUB);
    NODE_DEFINE_CONSTANT(target, ZMQ_SUB);
    NODE_DEFINE_CONSTANT(target, ZMQ_REQ);
    NODE_DEFINE_CONSTANT(target, ZMQ_XREQ);
    NODE_DEFINE_CONSTANT(target, ZMQ_REP);
    NODE_DEFINE_CONSTANT(target, ZMQ_XREP);
    NODE_DEFINE_CONSTANT(target, ZMQ_DEALER);
    NODE_DEFINE_CONSTANT(target, ZMQ_ROUTER);
    NODE_DEFINE_CONSTANT(target, ZMQ_PUSH);
    NODE_DEFINE_CONSTANT(target, ZMQ_PULL);
    NODE_DEFINE_CONSTANT(target, ZMQ_PAIR);

    NODE_DEFINE_CONSTANT(target, ZMQ_HWM);
    NODE_DEFINE_CONSTANT(target, ZMQ_SWAP);
    NODE_DEFINE_CONSTANT(target, ZMQ_AFFINITY);
    NODE_DEFINE_CONSTANT(target, ZMQ_IDENTITY);
    NODE_DEFINE_CONSTANT(target, ZMQ_SUBSCRIBE);
    NODE_DEFINE_CONSTANT(target, ZMQ_UNSUBSCRIBE);
    NODE_DEFINE_CONSTANT(target, ZMQ_RATE);
    NODE_DEFINE_CONSTANT(target, ZMQ_RECOVERY_IVL);
    NODE_DEFINE_CONSTANT(target, ZMQ_MCAST_LOOP);
    NODE_DEFINE_CONSTANT(target, ZMQ_SNDBUF);
    NODE_DEFINE_CONSTANT(target, ZMQ_RCVBUF);
    NODE_DEFINE_CONSTANT(target, ZMQ_RCVMORE);
    NODE_DEFINE_CONSTANT(target, ZMQ_FD);
    NODE_DEFINE_CONSTANT(target, ZMQ_EVENTS);
    NODE_DEFINE_CONSTANT(target, ZMQ_TYPE);
    NODE_DEFINE_CONSTANT(target, ZMQ_LINGER);
    NODE_DEFINE_CONSTANT(target, ZMQ_RECONNECT_IVL);
    NODE_DEFINE_CONSTANT(target, ZMQ_BACKLOG);

    NODE_DEFINE_CONSTANT(target, ZMQ_POLLIN);
    NODE_DEFINE_CONSTANT(target, ZMQ_POLLOUT);
    NODE_DEFINE_CONSTANT(target, ZMQ_POLLERR);

    NODE_DEFINE_CONSTANT(target, ZMQ_SNDMORE);
    NODE_DEFINE_CONSTANT(target, ZMQ_NOBLOCK);

    NODE_DEFINE_CONSTANT(target, STATE_READY);
    NODE_DEFINE_CONSTANT(target, STATE_BUSY);
    NODE_DEFINE_CONSTANT(target, STATE_CLOSED);

    Context::Initialize(target);
    Socket::Initialize(target);
  }
} // namespace zmq

// module

extern "C" void
init(Handle<Object> target) {
  zmq::Initialize(target);
}
