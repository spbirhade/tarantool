import os

import shutil
import subprocess
import yaml
import ConfigParser
from tarantool_server import TarantoolServer, TarantoolConfigFile
from tarantool_admin import TarantoolAdmin
from box import Box

class TarantoolBoxServer(TarantoolServer):
  def __new__(cls, core="tarantool", module="box"):
    return TarantoolServer.__new__(cls)

  def __init__(self, core="tarantool", module="box"):
    TarantoolServer.__init__(self, core, module)

  def configure(self, config):
    TarantoolServer.configure(self, config)
    with open(self.config) as fp:
      dummy_section_name = "tarantool"
      config = ConfigParser.ConfigParser()
      config.readfp(TarantoolConfigFile(fp, dummy_section_name))
      self.primary_port = int(config.get(dummy_section_name, "primary_port"))
      self.admin_port = int(config.get(dummy_section_name, "admin_port"))
      self.port = self.admin_port
      self.admin = TarantoolAdmin("localhost", self.admin_port)
      self.sql = Box("localhost", self.primary_port)


  def install(self, binary=None, vardir=None, mem=None, silent=True):
    super(self.__class__, self).install(binary, vardir, mem, silent)
    shutil.copy(os.path.join(self.builddir, "..", "..", "mod", "box", "lua", "box.lua"),
                os.path.join(self.vardir, "scripts", "lua"))
    shutil.copy(os.path.join(self.builddir, "..", "..", "mod", "box", "lua", "box_prelude.lua"),
                os.path.join(self.vardir, "scripts", "lua"))
    if not os.access(os.path.join(self.vardir, "scripts", "lua", "box"), os.F_OK):
      shutil.copytree(os.path.join(self.builddir, "..", "..", "mod", "box", "lua", "box"),
                      os.path.join(self.vardir, "scripts", "lua", "box"))

  def init(self):
# init storage
    subprocess.check_call([self.binary, "--init_storage"],
                          cwd = self.vardir,
# catch stdout/stderr to not clutter output
                          stdout = subprocess.PIPE,
                          stderr = subprocess.PIPE)

