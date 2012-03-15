import Options
from os import unlink, link
from os.path import exists 

APPNAME = 'zeromq.node'
VERSION = "1.0.0"

def set_options(opt):
    opt.tool_options("compiler_cxx")

def configure(conf):
    conf.check_tool("compiler_cxx")
    conf.check_tool("node_addon")
    conf.check_cfg(atleast_pkgconfig_version='0.0.0', mandatory=True, errmsg='pkg-config was not found')
    conf.check_cfg(package='libzmq', atleast_version='2.1.0', uselib_store='ZMQ', args='--cflags --libs', mandatory=True, errmsg='Package libzmq was not found')
    conf.check(lib='uuid', uselib_store='UUID')

def build(bld):
    obj = bld.new_task_gen("cxx", "shlib", "node_addon")
    obj.target = "binding"
    obj.source = "binding.cc"
    obj.uselib = "ZMQ UUID"

def shutdown():
  # HACK to get binding.node out of build directory.
  # better way to do this?
  if exists('./binding.node'):
    unlink('./binding.node')
  if Options.commands['build']:
    if exists('./build/default/binding.node'):
      link('./build/default/binding.node', './binding.node')
    elif exists('./build/Release/binding.node'):
      link('./build/Release/binding.node', './binding.node')
    else:
      raise Exception("Cannot locate build binding.node")

