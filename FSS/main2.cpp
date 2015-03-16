#include <iostream>
#include <string> // for strcmp
#include "fss.h"
using namespace std;

int main(int argc, char *argv[]) {
	string metaAddr = "localhost:4000";

  cout << "INITIALIZING FSS" << endl;

	if(argc > 1) {
		 string temp(argv[1]);
		 metaAddr = temp;
	}

  cout << "meta ip: " << metaAddr << endl;
	FSS* fss = new FSS(metaAddr);

  if (argc > 2)
    fss.fss_addr = argv[2];

	fss->listen();
	return 0;
}
