# linux_shell
## Initial Version
Only contain some basic callings to C functions

## Install SIGINT signal 
Before while loop, ask the main function to ignore SIGINT
After fork the child process, restore it to default

## Use non-local jump
To start a new while loop and thus print a new line
on pressing Ctrl-C

## Use sigaction to better handle signal

## Add Ctrl-D funtion
Terminate the shell

## Add redirection
Only support standard command:
command > file  -- output_redirect
command < file  -- input_redirect

## Add pipeline
e.g. echo "Hello" | cat
Question remain to solve:
Why it cannot run correctly when two child process are created?
As in the reference web: https://panqiincs.me/2017/04/19/write-a-shell-redirect-and-pipeline/
When debugging, it shows that only the command1 process terminated
