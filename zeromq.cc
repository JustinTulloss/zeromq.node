#include <stdio.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <zmq.h>
#include <assert.h>

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
        context_ = zmq_init(1, 1, ZMQ_POLL);
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

        NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

        target->Set(String::NewSymbol("Socket"), t->GetFunction());
    }

    void Close(Local<Value> exception = Local<Value>()) {
        zmq_close(socket_);
        socket_ = NULL;
        Unref();
    }

protected:
    static Handle<Value>
    New (const Arguments &args) {
        HandleScope scope;
        Context *context = ObjectWrap::Unwrap<Context>(args[0]->ToObject());

        Socket *socket = new Socket(context);
        socket->Wrap(args.This());

        return args.This();
    }

    static Handle<Value>
    Close (const Arguments &args) {
        Socket *socket = ObjectWrap::Unwrap<Socket>(args.This());
        HandleScope scope;
        socket->Close();
        return Undefined();
    }

    Socket (Context *context) : EventEmitter () {
        context_ = context;
        socket_ = zmq_socket(context->getCContext(), ZMQ_P2P);
    }

    ~Socket () {
        assert(socket_ == NULL);
    }

private:
    void *socket_;
    void *context_;
};
}

extern "C" void
init (Handle<Object> target) {
    HandleScope scope;
    zmq::Context::Initialize(target);
    zmq::Socket::Initialize(target);
}
