#pragma comment(lib, "msvcrt.dll")
#pragma comment(lib, "SDL2.dll")
#pragma comment(lib, "opengl32.dll")

typedef struct
{
	int type;
	char rest[54];
} SDL_Event;

render()
{
	glClearColor(0.0,1.0,0.0,1.0);
	glClear(0x4000); //color buffer bit
}

main() {
	i = SDL_Init(0x20); //init video
	$printf("i = %d\n",i);
	
	wnd = SDL_CreateWindow("test", 100,100,512,512, 2); //2 = gl
	glctx = SDL_GL_CreateContext(wnd);
	SDL_GL_MakeCurrent(wnd,glctx);
	
	ev = new SDL_Event;
	ev.type=0;
		
	running=true;
	
	while(running != false)
	{
		while(SDL_PollEvent(ev) != 0)
		{
			printf("ev types = %\n",ev.type);
			if(ev.type == 0x100) //sdl_quit
			{
				running=false;
			}
			
			if(ev.type==0x300) //sdl_keydown
			{
				printf("DOWN!\n");
			}
		}
		render();
		SDL_GL_SwapWindow(wnd);
	}
	
	100:
	
	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(wnd);
	SDL_Quit();
}