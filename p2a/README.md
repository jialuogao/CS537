this project is a shell that can use user input as well as read from an input file
	- run by the command "./wish"
	- the shell includes built in functions including exit, cd, history wich shows input history
	- history is implemented by using an array of  char* and when ever the array size reaches the limit, the array grow by 10 in size
	- the path function changes the pathes stored in an array
this shell also support redirection >
	- redirecte can write output into a file instead of stdin, dup2 and open are used to achive this
	- <command> <argument> ...  >  <output file>
piping |
	- piping can directly connect the output of command 1 into the input of command 2, dup2 and pipe are used
	- <command1> <arguments> ... | <command2> <arguments> ...
