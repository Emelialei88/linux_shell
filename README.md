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
