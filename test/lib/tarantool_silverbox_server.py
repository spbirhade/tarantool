import shutil
import subprocess
import yaml
import ConfigParser
from tarantool_server import TarantoolServer, TarantoolConfigFile
from tarantool_admin import TarantoolAdmin
from silverbox import Silverbox

class TarantoolSilverboxServer(TarantoolServer):
  def __new__(cls, core="tarantool", module="silverbox"):
    return TarantoolServer.__new__(cls)

  def __init__(self, core="tarantool", module="silverbox"):
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
      self.sql = Silverbox("localhost", self.primary_port)

  def init(self):
# init storage
    subprocess.check_call([self.binary, "--init_storage"],
                          cwd = self.vardir,
# catch stdout/stderr to not clutter output
                          stdout = subprocess.PIPE,
                          stderr = subprocess.PIPE)

