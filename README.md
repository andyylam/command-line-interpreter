# Command Line Interpreter in C

A command line interpreter (aka command prompt or shell) allows the execution of multiple user commands with various number of arguments. The user inputs the commands one after another, and the commands are executed by the command line interpreter. This is a Linux command line interpreter written in C, enabled by system calls (forks and pipes).

## Possible user requests:

1. Quit the interpreter (a.k.a. put Genie back in the bottle).
    - Format: `quit`
2. Run a command (ask your Genie to fulfill a wish).
    - Format: `command [arg1 [arg2 [arg3 …]]]`
3. Pipe between two commands.
    - Format:
      `command1 [arg1 to arg10] | command2 [arg1 to arg10]`
4. Pipes among multiple commands. - Format:
    - `command1 [arg1 to arg10] | command2 [arg1 to arg10] | command3 [arg1 to arg10] ...`
5. Set an environment variable.
    - Format: `set NAME value`
6. Unset an environment variable.
    - Format: `unset $NAME`
7. Use an environment variable in any command.
    - Format: `$NAME`
