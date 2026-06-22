#!/usr/bin/env python3
import os
import shlex
import sys
import subprocess
from collections import OrderedDict
from dataclasses import dataclass
from typing import Callable, Optional
from prompt_toolkit import PromptSession
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.patch_stdout import patch_stdout
from rich.console import Console
import pyfiglet

console = Console()

banner = "M - STEP"
welcome_message = "Welcome to the M-Step Framework CLI."
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
RUN_SCRIPT = os.path.join(BASE_DIR, "../scripts", "run.sh")

STYLES = {
    "banner": "bold cyan",
    "welcome": "dim",
    "hint": "default",
    "exit": "yellow",
    "success": "green",
    "warning": "yellow",
    "error": "red",
    "section": "bold",
    "command": "cyan",
}


@dataclass(frozen=True)
class CommandSpec:
    description: str
    handler: Callable[[list[str]], None]


def style_text(kind: str, message: str) -> str:
    return f"[{STYLES[kind]}]{message}[/{STYLES[kind]}]"

def print_header():
    """Prints the large ASCII art header."""
    ascii_art = pyfiglet.figlet_format(banner) 
    console.print(style_text("banner", ascii_art))
    console.print(welcome_message, style=STYLES["welcome"])
    console.print("Type [bold]/help[/bold] for commands, or 'exit' to.\n")

def cmd_exit(_args: list[str]) -> None:
    console.print(style_text("exit", "Exiting M-Step CLI..."))
    sys.exit(0)


def cmd_run(args: list[str]) -> None:
    if not os.path.exists(RUN_SCRIPT):
        console.print(style_text("error", f"Run script not found: {RUN_SCRIPT}"))
        console.print()
        return

    if not os.access(RUN_SCRIPT, os.X_OK):
        console.print(style_text("error", f"Run script is not executable: {RUN_SCRIPT}"))
        console.print()
        return

    completed = subprocess.run([RUN_SCRIPT, *args], check=False)
    exit_code = completed.returncode
    if exit_code == 0:
        console.print(style_text("success", "Pipeline started successfully."))
    else:
        console.print(style_text("error", f"Pipeline failed with exit code {exit_code}."))
    console.print()


def cmd_status(_args: list[str]) -> None:
    exists = os.path.exists(RUN_SCRIPT)
    executable = os.access(RUN_SCRIPT, os.X_OK)

    console.print(style_text("section", "Framework Status:"))
    console.print(
        f"  {style_text('command', 'Run script')} : "
        f"{'available' if exists else 'missing'}"
    )
    console.print(
        f"  {style_text('command', 'Script path')} : {RUN_SCRIPT}"
    )
    console.print(
        f"  {style_text('command', 'Executable')} : "
        f"{'yes' if executable else 'no'}"
    )
    console.print()


COMMANDS: "OrderedDict[str, CommandSpec]" = OrderedDict()


def cmd_help(_args: list[str]) -> None:
    console.print("\n" + style_text("section", "Available Commands:"))
    for command_name, spec in COMMANDS.items():
        console.print(
            f"  {style_text('command', command_name):<22} - {spec.description}"
        )
    console.print()

def cmd_TBD(_args: list[str]) -> None:
    console.print("\n" + style_text("section", "Command TBD"))


def prompt_evaluation_target() -> Optional[str]:
    options = [
        ("1", "t1", "mstp-metrics"),
        ("2", "t2", "covert-udiv"),
        ("3", "t3", "covert-inst"),
        ("4", "t4", "covert-cache"),
        ("5", "t5", "covert-cont"),
        ("6", "t6", "pocs"),
        ("7", "t7", "printf-gtkwave"),
        ("8", "ALL", "run all evaluation tests"),
    ]
    selectors = {number: target for number, target, _ in options}
    selectors.update({
        "t1": "t1",
        "t2": "t2",
        "t3": "t3",
        "t4": "t4",
        "t5": "t5",
        "t6": "t6",
        "t7": "t7",
        "all": "ALL",
    })

    console.print(style_text("section", "Run Evaluation:"))
    for number, target, description in options:
        console.print(f"  [{number}] {target:<3} - {description}")
    console.print("Choose one option (1-8 or t1..t7/ALL). Press Enter to cancel.")

    session = PromptSession()
    while True:
        try:
            raw_choice = session.prompt("evaluation> ")
        except KeyboardInterrupt:
            console.print(style_text("warning", "Evaluation cancelled."))
            return None
        except EOFError:
            console.print(style_text("warning", "Evaluation cancelled."))
            return None

        choice = raw_choice.strip().lower()
        if not choice:
            console.print(style_text("warning", "Evaluation cancelled."))
            return None

        selected = selectors.get(choice)
        if selected is not None:
            return selected

        console.print(style_text("error", f"Invalid option: {raw_choice.strip()}"))
        console.print("Try again with 1-8, t1..t7, or ALL.")

def cmd_quick_start(args: list[str]) -> None:
    RUN_SCRIPT_QS = os.path.join(BASE_DIR, "../scripts", "cmd-quick-start", "run.sh")
    if not os.path.exists(RUN_SCRIPT_QS):
        console.print(style_text("error", f"Run script not found: {RUN_SCRIPT_QS}"))
        console.print()
        return

    if not os.access(RUN_SCRIPT_QS, os.X_OK):
        console.print(style_text("error", f"Run script is not executable: {RUN_SCRIPT_QS}"))
        console.print()
        return

    completed = subprocess.run([RUN_SCRIPT_QS, *args], check=False)
    exit_code = completed.returncode
    if exit_code != 0:
        console.print(style_text("error", f"Pipeline failed with exit code {exit_code}."))
    console.print()
    
def cmd_evaluation(args: list[str]) -> None:
    RUN_SCRIPT_EVAL = os.path.join(BASE_DIR, "../scripts", "cmd-evaluation", "run.sh")
    if not os.path.exists(RUN_SCRIPT_EVAL):
        console.print(style_text("error", f"Run script not found: {RUN_SCRIPT_EVAL}"))
        console.print()
        return

    if not os.access(RUN_SCRIPT_EVAL, os.X_OK):
        console.print(style_text("error", f"Run script is not executable: {RUN_SCRIPT_EVAL}"))
        console.print()
        return

    evaluation_args = list(args)
    if not evaluation_args:
        selection = prompt_evaluation_target()
        if selection is None:
            console.print()
            return

        evaluation_args.append(selection)

    completed = subprocess.run([RUN_SCRIPT_EVAL, *evaluation_args], check=False)
    exit_code = completed.returncode
    if exit_code != 0:
        console.print(style_text("error", f"Pipeline failed with exit code {exit_code}."))
    console.print()
    
COMMANDS.update(
    {
        "/help": CommandSpec("Show this message", cmd_help),
        "/run": CommandSpec("Execute the main M-Step pipeline", cmd_run),
        "/status": CommandSpec("Check framework status", cmd_status),
        "/qs": CommandSpec("Quick start the framework `Hello World`", cmd_quick_start),
        "/evaluation": CommandSpec("Run evaluation (text menu or /evaluation t1..t7|ALL)", cmd_evaluation),
        # "/profiles": CommandSpec("Command TBD 2", cmd_TBD),
        "exit": CommandSpec("Exit the CLI", cmd_exit),
    }
)

def execute_command(command: str) -> None:
    """Routes user input using a command registry."""
    raw = command.strip()
    if not raw:
        return

    try:
        tokens = shlex.split(raw)
    except ValueError as exc:
        console.print(style_text("error", f"Invalid command syntax: {exc}"))
        console.print()
        return

    if not tokens:
        return

    cmd = tokens[0].lower()
    args = tokens[1:]

    spec: Optional[CommandSpec] = COMMANDS.get(cmd)
    if spec is None:
        console.print(style_text("error", f"Unknown command: {cmd}"))
        console.print()
        return

    spec.handler(args)

def main():
    print_header()
    
    # Create the persistent session
    session = PromptSession()

    while True:
        try:
            # patch_stdout ensures background processes don't break the prompt visual
            with patch_stdout():
                # This creates the "> " prompt and the bottom status bar
                user_input = session.prompt(
                    HTML('<ansimagenta>></ansimagenta> '), 
                    # bottom_toolbar=bottom_toolbar
                )
            execute_command(user_input)
                
        except KeyboardInterrupt:
            continue  # Ctrl+C clears the current line instead of exiting
        except EOFError:
            break     # Ctrl+D exits

if __name__ == '__main__':
    main()