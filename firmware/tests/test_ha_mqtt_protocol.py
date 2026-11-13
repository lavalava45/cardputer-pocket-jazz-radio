"""Compile and execute the portable Home Assistant MQTT codec with live asserts."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class HomeAssistantProtocolHostTests(unittest.TestCase):
    def test_cxx11_host_protocol_with_assertions_enabled(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        use_wsl = compiler is None and os.name == "nt" and shutil.which("wsl.exe") is not None
        if compiler is None and not use_wsl:
            self.skipTest("C++11 host compiler not installed")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("ha_mqtt_protocol.exe" if os.name == "nt" else "ha_mqtt_protocol")
            source = str(ROOT / "tests" / "ha_mqtt_protocol.cpp")
            if use_wsl:
                def linux_path(path):
                    return subprocess.run(
                        ["wsl.exe", "--exec", "wslpath", "-a", path],
                        check=True, capture_output=True, text=True, timeout=20,
                    ).stdout.strip()

                linux_source, linux_executable = linux_path(source), linux_path(str(executable))
                compile_command = ["wsl.exe", "--exec", "g++", "-std=c++11", "-Wall", "-Wextra",
                                   "-Werror", "-O2", "-DNDEBUG", linux_source, "-o", linux_executable]
                run_command = ["wsl.exe", "--exec", linux_executable]
            else:
                compile_command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                                   "-O2", "-DNDEBUG", source, "-o", str(executable)]
                run_command = [str(executable)]
            subprocess.run(compile_command, check=True, capture_output=True, text=True, timeout=45)
            subprocess.run(run_command, check=True, capture_output=True, text=True, timeout=20)


if __name__ == "__main__":
    unittest.main()
