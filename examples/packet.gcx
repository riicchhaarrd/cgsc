
main() {
	//echo -e "\xFF\xFF\xFF\xFFgetservers 118 full empty" | nc cod2master.activision.com -u 20710
	printf("packet test\n");
	master="185.34.104.231:20710";
	
	req="\xff\xff\xff\xffgetservers 118 full empty";
	ret = sendpacket(master, req, req.size);
	printf("ret = %", ret);
}