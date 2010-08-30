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
#include <unistd.h>

using namespace v8;
using namespace node;

namespace zmq {

zmq_msg_t z_msg;

static Persistent<String> receive_symbol;
static Persistent<String> error_symbol;
static Persistent<String> connect_symbol;

//Forward declare cause they kind of interdepend
class Socket;
class Context;

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

    void AddSocket(Socket *s) {
        sockets_.push_back(s);
        zmq_poller_.data = this;
        ev_idle_start(EV_DEFAULT_UC_ &zmq_poller_);
    }

    void RemoveSocket(Socket *s) {
        sockets_.remove(s);
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
        ev_idle_init(&zmq_poller_, DoPoll);
    }

    ~Context () {
        assert(context_ == NULL);
    }

private:

    static void DoPoll(EV_P_ ev_idle *watcher, int revents) {
        std::list<Socket *>::iterator s;
        assert(revents == EV_IDLE);

        printf("Do Poll!\n");

        Context *c = (Context *) watcher->data;

        int i = -1;

        zmq_pollitem_t *pollers = (zmq_pollitem_t *)
            malloc(c->sockets_.size() * sizeof(zmq_pollitem_t));
        if (!pollers) return;

        for (s = c->sockets_.begin(); s != c->sockets_.end(); s++) {
            i++;
            Socket *k = *s;
            pollers[i].socket = k->socket_;
            pollers[i].events = k->events_;
        }

        zmq_poll(pollers, i + 1, 0); // Return instantly w/timeout 0

        i = -1;

        for (s = c->sockets_.begin(); s != c->sockets_.end(); s++) {
            i++;
            //s->AfterPoll(pollers[i]->revents);
        }
    }

    ev_idle zmq_poller_;
    void * context_;
    std::list<Socket *> sockets_;
};


class Socket : public EventEmitter {
friend class Context;
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
        return zmq_bind(socket_, address);
    }

    int Connect(const char *address) {
        return zmq_connect(socket_, address);
    }

    int Send(char *msg, int length, int flags, void* hint) {
        int rc;
        rc = zmq_msg_init_data(&z_msg, msg, length, FreeMessage, hint);
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

    void Close() {
        zmq_close(socket_);
        context_->RemoveSocket(this);
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
        if (socket->Connect(*address)) {
            return ThrowException(Exception::Error(
                String::New(socket->ErrorMessage())));
        }
        return Undefined();
    }

    static Handle<Value>
    Bind (const Arguments &args) {
        HandleScope scope;
        Socket *socket = getSocket(args);
        if (args[0]->IsString()) {
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
        context_ = context;
    }

    ~Socket () {
        assert(socket_ == NULL);
    }

private:

    void QueueOutgoingMessage(Local <Value> message) {
        Persistent<Value> p_message = Persistent<Value>::New(message);
        events_ |= ZMQ_POLLOUT;
        context_->AddSocket(this);
        outgoing_.push_back(p_message);
    }

    static void FreeMessage(void *data, void *message) {
        String::Utf8Value *js_msg = (String::Utf8Value *)message;
        delete js_msg;
    }

    static Socket * getSocket(const Arguments &args) {
        return ObjectWrap::Unwrap<Socket>(args.This());
    }

    int AfterPoll(int revents) {
        HandleScope scope;

        printf("got events: %x!\n", revents);

        Local <Value> exception;

        if (revents & ZMQ_POLLIN) {
            zmq_msg_t z_msg;
            if (Recv(0, &z_msg)) {
                Emit(error_symbol, 1, &exception);
            }
            Local <Value>js_msg = String::New(
                (char *) zmq_msg_data(&z_msg),
                zmq_msg_size(&z_msg));
            Emit(receive_symbol, 1, &js_msg);
            zmq_msg_close(&z_msg);

        }

        if (revents & ZMQ_POLLOUT && !outgoing_.empty()) {
            String::Utf8Value *message = new String::Utf8Value(outgoing_.front());
            if (Send(**message, message->length(), 0, (void *) message)) {
                exception = Exception::Error(
                    String::New(ErrorMessage()));
                Emit(receive_symbol, 1, &exception);
            }

            outgoing_.front().Dispose();
            outgoing_.pop_front();

            if (outgoing_.empty()) {
                printf("removing socket\n");
                context_->RemoveSocket(this);
            }
        }

        return 0;
    }

    void *socket_;
    Context *context_;
    short events_;
    std::list< Persistent <Value> > outgoing_;
};

}

extern "C" void
init (Handle<Object> target) {
    HandleScope scope;
    zmq::Context::Initialize(target);
    zmq::Socket::Initialize(target);
}
