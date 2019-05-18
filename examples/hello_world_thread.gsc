another_pseudo_thread() {
	while(level.exit == false) {
		print("waited 2\n");
		wait 2;
	}
}

main() {
	level.exit=false;
	thread another_pseudo_thread();
	for(i = 0; i < 10; i++) {
		print("i = "+i+"\n");
		wait 1;
	}
	level.exit=true;
}