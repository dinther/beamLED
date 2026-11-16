# PlatformIO pre-script: fetch Ableton Link (unmodified, same tag as Glow) into .pio/deps/link and
# add its headers. Link is used under Ableton's licence agreement; it is never committed here.
Import("env")
import os
import subprocess

TAG = "Link-4.1"
dest = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "deps", "link")
if not os.path.isfile(os.path.join(dest, "include", "ableton", "Link.hpp")):
    print("Fetching Ableton Link %s" % TAG)
    subprocess.check_call(["git", "clone", "--depth", "1", "--branch", TAG,
                           "https://github.com/Ableton/link.git", dest])
env.Append(CPPPATH=[os.path.join(dest, "include")])
