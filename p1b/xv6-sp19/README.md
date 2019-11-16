to implement getopenedcount() and getclosedcount(), multiple lines of code are added to multiple files including sysfunc.h and some kernel files
getopenedcount() function returns the total numbers of calls to the open function
getclosedcount() function returns the total numbers of calls to the close function
two counters are added to the file sysfile.c to record 2 counts mentioned above
the open and close functions in sysfile.c are modified to increment the counts
new functions sys_getopenedcount and sys_getclosedcount are added to return the counts
