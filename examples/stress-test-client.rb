# Stress test client.
# See stress-test-server.js for commentary.

begin
  require 'rubygems'
rescue LoadError
  # try to proceed without rubygems
end

require 'ffi-rzmq'

THREADS = 10
INFINITY = true
HOST = 'localhost'

ctx = ZMQ::Context.new(1)

begin
  threads = (1..THREADS).map do |i|
    Thread.new do
      socket = ctx.socket(ZMQ::REQ)
      socket.connect("tcp://#{HOST}:23456")
      socket.send_string "hello! #{i}"
      response = socket.recv_string
      $stdout << "thread #{i} got #{response}\n"
      socket.close
    end
  end

  threads.each{|t| t.join}
end while INFINITY
