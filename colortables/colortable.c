int main(){
	int red,green,blue;
	red=0;
	green=0;
	blue=0;
	for(blue=0;blue<=254;blue++){
		printf("{%d,%d,%d},\n",red,green,blue);
	}
	blue=254;
	for(green=0;green<=254;green++){
		printf("{%d,%d,%d},\n",red,green,blue);
	}
	green=254;
	for(blue=254;blue>=0;blue--){
		printf("{%d,%d,%d},\n",red,green,blue);
	}
	blue=0;
	green=254;
	for(red=0;red<=254;red++){
		printf("{%d,%d,%d},\n",red,green,blue);
	}
	red=254;
	for(green=254;green>=0;green--){
		printf("{%d,%d,%d},\n",red,green,blue);
	}
}
