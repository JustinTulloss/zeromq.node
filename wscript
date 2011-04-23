import Options
from os import unlink, link
from os.path import exists 

APPNAME = 'zeromq.node'
VERSION = "0.4.0"

def set_options(opt):
    opt.tool_options("compiler_cxx")

def configure(conf):
    conf.check_tool("compiler_cxx")
    conf.check_tool("node_addon")
    conf.check_cfg(package='libzmq', args='--cflags --libs "libzmq >= 2.1.0"')

def build(bld):
    obj = bld.new_task_gen("cxx", "shlib", "node_addon")
    obj.cxxflags = ["-Wall", "-Werror"]
    obj.target = "binding"
    obj.source = "binding.cc"
    obj.lib = ["zmq"]

def shutdown():
  # HACK to get binding.node out of build directory.
  # better way to do this?
  if exists('./binding.node'): unlink('./binding.node')
  if Options.commands['build']:
    link('./build/default/binding.node', './binding.node')
