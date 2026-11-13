"""C++11 host assertions for bounded forwarding and mute/notification policy."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class ForwardingPolicyHostTests(unittest.TestCase):
    def test_forwarding_and_notice_rate_with_release_assertions(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        use_wsl = compiler is None and os.name == "nt" and shutil.which("wsl.exe") is not None
        if compiler is None and not use_wsl:
            self.skipTest("C++11 host compiler not installed")
        with tempfile.TemporaryDirectory() as directory:
            source = str(Path(__file__).resolve().parent / "ha_forward_policy.cpp")
            executable = str(Path(directory) / ("ha_forward_policy.exe" if os.name == "nt" else "ha_forward_policy"))
            flags = ["-std=c++11", "-Wall", "-Wextra", "-Werror", "-O2", "-DNDEBUG"]
            if use_wsl:
                def linux_path(path):
                    return subprocess.run(["wsl.exe", "--exec", "wslpath", "-a", path],
                                          check=True, capture_output=True, text=True,
                                          timeout=20).stdout.strip()

                linux_exe = linux_path(executable)
                compile_cmd = ["wsl.exe", "--exec", "g++", *flags, linux_path(source), "-o", linux_exe]
                run_cmd = ["wsl.exe", "--exec", linux_exe]
            else:
                compile_cmd = [compiler, *flags, source, "-o", executable]
                run_cmd = [executable]
            subprocess.run(compile_cmd, check=True, capture_output=True, text=True, timeout=45)
            subprocess.run(run_cmd, check=True, capture_output=True, text=True, timeout=20)


if __name__ == "__main__":
    unittest.main()
