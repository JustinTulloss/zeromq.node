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

#ifndef ZEROMQ_NODE
#define ZEROMQ_NODE

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

class Context;

class Socket : public EventEmitter {
friend class Context;
public:
    static void Initialize (v8::Handle<v8::Object>);
    int Bind(const char *);
    int Connect(const char *);
    int Send(char *msg, int length, int flags, void* hint);
    int Recv(int flags, zmq_msg_t* z_msg);
    void Close();
    const char * ErrorMessage();

protected:
    static Handle<Value> New (const Arguments &);
    static Handle<Value> Connect (const Arguments &);
    static Handle<Value> Bind (const Arguments &);
    static Handle<Value> Send (const Arguments &);
    static Handle<Value> Close (const Arguments &);
    Socket(Context *, int);
    ~Socket();

private:
    void QueueOutgoingMessage(Local <Value>);
    int AfterPoll(int);

    static void FreeMessage(void *data, void *message);
    static Socket * getSocket(const Arguments &);

    void *socket_;
    Context *context_;
    short events_;
    std::list< Persistent <Value> > outgoing_;
};

class Context : public EventEmitter {
public:
    static void Initialize (v8::Handle<v8::Object>);
    void *getCContext();
    void AddSocket(Socket *);
    void RemoveSocket(Socket *);
    void Close();

protected:
    static Handle<Value> New(const Arguments&);
    static Handle<Value> Close(const Arguments&);
    Context();
    ~Context();

private:
    static void DoPoll(EV_P_ ev_idle *watcher, int revents);
    void * context_;
    std::list<Socket *> sockets_;
    ev_idle zmq_poller_;
};

}

#endif // ZEROMQ_NODE
