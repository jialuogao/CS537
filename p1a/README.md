my-cat.c:
	Read and print the files
	used fopen, fclose, and getline
my-sed.c:
	Read and print the files after replacing first "find" string 
	of each line with "replace" string
	if no files are given, use stdin
	used fopen, fclose, getline
	function "mysed(line, find, replace)" is used to do the replacement
		the location of "find" string is found by using strcpy
		and then the part of string after "find" is stored in var 
		"half" then the location of "find" is replaced with a "\0"
		to terminate the string. The string is then combined using
		strcat(line, replace); strcat(line, half);
	
my-uniq.c:
	Read and print the files after removing duplicate lines
	if no files are given, use stdin
	used fopen, fclose, getline
	the first line is read and stored in "buffernew"
	used strcmp to compare strings
	if "buffer" is not equal to "buffernew" print the line
	then copy "buffernew" to "buffer" using strcpy
	clear "buffer" to "\0" before reading from new files
