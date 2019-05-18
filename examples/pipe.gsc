#pragma comment(lib, "/lib/i386-linux-gnu/libc.so.6")
#define STDOUT_FILENO 1
typedef struct
{
	int out;
	int in;
} filedes_t;

main()
{
	filedes = new filedes_t;

	if (pipe(filedes) == -1) {
		$perror("pipe");
		$exit(1);
	}

	pid = fork();
	if (pid == -1) {
		$perror("fork");
		$exit(1);
	} else if(pid == 0)
	{
		printf("pid = %\n",pid);
		dup2(filedes->in, STDOUT_FILENO);
		printf("hello world\n");
		$shutdown(filedes->out, 1);
		close(filedes->in);
		close(filedes->out);
		$_exit(1); //exit child process
	}
	close(filedes->in);
	buf = buffer(4096);
	$memset(buf,0,buf.size);
	brk = false;
	while(!brk)
	{
		count = read(filedes->out, buf, buf.size);
		if(count == 0)
		{
			brk = true;
		} else
		{
			$printf("buf=%s,count=%d\n",buf,count);
		}
	}
}