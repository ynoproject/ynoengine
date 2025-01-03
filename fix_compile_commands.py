import os
import sys
import time
import json
import subprocess

from typing import TypedDict

class CompileCommand(TypedDict):
    directory: str
    command: str
    file: str
    output: str

if __name__ == "__main__":
    timeout = 5.0
    compile_commands_json = "compile_commands.json"
    extra_flags = " " + os.environ["EXTRA_FLAGS"]

    if len(sys.argv) > 1 and sys.argv[1] == "--watcher":
        file = open("fix_compile_commands.log", "w", buffering=1)
        sys.stdout = file
        sys.stderr = file

        started = time.time()
        print(f"Waiting for {compile_commands_json} to be created...")
        while not os.path.exists(compile_commands_json):
            time.sleep(0.1)
            if time.time() - started > timeout:
                print(f"File not created within {timeout} seconds: {compile_commands_json}")
                sys.exit(1)

        while True:
            json_contents = ""
            try:
                with open(compile_commands_json) as fp:
                    json_contents = fp.read().strip()
            except Exception as x:
                print(f"Could not read {compile_commands_json}: {x}")

            elapsed = time.time() - started
            if json_contents.endswith("]"):
                elapsed = time.time() - started
                print(f"File finished in {elapsed:.2f} seconds: {compile_commands_json}")
                break

            if elapsed > timeout:
                print(f"File not finished within {timeout} seconds: {compile_commands_json}")
                sys.exit(1)

            time.sleep(0.1)

        # Append the extra flags to the command
        os.remove(compile_commands_json)
        compile_commands: list[CompileCommand] = json.loads(json_contents)
        print(f"before: {json.dumps(compile_commands, indent=2)}")
        for compile_command in compile_commands:
            compile_command["command"] += extra_flags
        print(f"after: {json.dumps(compile_commands, indent=2)}")

        with open(compile_commands_json, "w") as fp:
            json.dump(compile_commands, fp, indent=2)
    else:
        if os.path.exists(compile_commands_json):
            os.remove(compile_commands_json)
        compile_command = [sys.executable, __file__, "--watcher"]
        subprocess.Popen(
            compile_command,
            start_new_session=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
