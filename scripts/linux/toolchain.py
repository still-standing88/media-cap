import shutil
import shlex
import subprocess


def prepare(env, flexaudio, issues):
    path = env.get("PATH", "")
    pkg_config = shutil.which(env.get("PKG_CONFIG", "pkg-config"), path=path)
    modules = ["x11", "xext", "xtst", "xfixes", "xinerama"]
    if flexaudio:
        modules.extend(("alsa", "libpipewire-0.3"))
    if not pkg_config:
        issues.append("Install pkg-config and the development packages for " + ", ".join(modules) + ".")
    else:
        missing = [name for name in modules
                   if subprocess.run([pkg_config, "--exists", name], env=env).returncode]
        if missing:
            issues.append("Install the development packages for " + ", ".join(missing) + ".")

    compiler = env.get("CXX") or "c++"
    command = [compiler] if shutil.which(compiler, path=path) else shlex.split(compiler)
    if command and shutil.which(command[0], path=path):
        result = subprocess.run([*command, "-E", "-x", "c++", "-"],
                                input="#include <linux/videodev2.h>\n",
                                text=True, capture_output=True, env=env)
        if result.returncode:
            issues.append("Install Linux userspace kernel headers for the V4L2 camera backend.")
