#include "CDN_Node.h"
#include "CDNReceiver.h"
#include "CDNSender.h"
#include "csv.h" //use CSV parser
#include "CDN_Cache.h"
#include "Shared.h"
#include <string>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;


CDN_Node::CDN_Node(string metaIpAddr, string fssIpAddr) {
	make_storage();
	m_metaIpAddr = metaIpAddr;
	m_fssIpAddr = fssIpAddr;
	m_sender = new CDNSender(metaIpAddr, fssIpAddr);
    
	//TODO: initialize m_address
    get_address();
    m_cdnId = -1;
    //m_sender->sendRegisterMsgToMeta(m_address, m_cdnId);
}

CDN_Node::~CDN_Node() {
	delete m_sender;
}

bool CDN_Node::make_storage() {
	strcpy(wd, CDN_DIR);
	mkdir(wd, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	return true;
}

void CDN_Node::get_address() {
    
    //set the external IpAddress and convert into number for location
    get_and_set_CDN_addr();
    
    /* get the gps information and assign them to Address */
    get_gps_info();
    
    m_address.latLng.first = cdn_gps.first;
    m_address.latLng.second = cdn_gps.second;
    //ip address is assigned in get_and_set_CDN_addr()

    cout<<"CDN IpAddr: "<<m_address.ipAddr<<endl;
    cout<<"CDN Lat: "<<m_address.latLng.first<<" ";
    cout<<"CDN Lng: "<<m_address.latLng.second<<endl;
}

//TODO: take relative file path instead of file name as the first argument !!
bool CDN_Node::look_up_and_remove_storage(string filename, int signal) {
		/*
			Given filename, and signal is 0, look up the storage of Node and return true if there is match.
		
            If signal is 1, look up the file and remove it from the directory.
         */
        
            //convert string to char
            const char* cstr = filename.c_str();
        
			if((dir = opendir(wd)) != NULL) {
				while ((ent = readdir(dir)) != NULL) {
					if (strcmp(ent->d_name,cstr)==0 && signal == 0) {
                        cout << cstr << " is located in this directory" << endl;
						return true;
                    } else if (strcmp(ent->d_name,cstr)==0 && signal == 1) {
                        //remove the file
                        char temp[256];
                        strcpy(temp,path_maker(cstr));
                        if(remove(temp) == 0) {
                            cout << "deletion successful" << endl;
                            return true;
                        } else {
                            cout << "Can't remove " << cstr << ": " << strerror(errno) << endl;
                        }
                    }
				}
				closedir (dir);
			} else {
				/* can not open directory */
				perror("Can't open the directory");
				return false;
			}

			return false;
}

bool CDN_Node::write_file(const string &contents, string filename, vector<string>& deletedfiles){
    
    //Configure the storage's capacity and free some portion if exceeds and insert the new file information
    long long file_size = contents.size();
    cout << "file size: " << file_size << endl;
    
    managing_files(filename, file_size, deletedfiles);
    
    //convert string to char
    const char* cstr = filename.c_str();
    char temp[256];
    strcpy(temp,path_maker(cstr));
    
    //Write to file
    ofstream f (temp);
    if(f.is_open()){
        f << contents;
        f.close();
    } else {
        cout << "Unable to write the file";
        return false;

    }
    return true;
}

string CDN_Node::load_file(string filename) {
    //convert string to char
    const char* cstr = filename.c_str();
    char temp[256];
    strcpy(temp,path_maker(cstr));
    //Return contents of file
    ifstream f (temp);
    if(f.is_open()) {
        //update cache information
        file_tracker.get(filename);
        
        stringstream buf;
        buf << f.rdbuf();
        return buf.str();
    } else {
        return "Can't load the file";
    }
}

/*
 Whenever we try to save the file into Directory, CDN takes the new file size and compare it whether
 the file size fits to the capacity or not. If it exceeds the limit, this function remove least used
 files until it satisfy the condition.
 */
void CDN_Node::managing_files(string filename,long long file_size, vector<string>& deletedfiles) {
	/*
		Manage the file storage to maintain its free-capacity.
	*/
    while((this->get_size_of_storage() + file_size) > storage_capacity) {
        cout << "current capacity: " << this->get_size_of_storage() << endl;
        string file_name = file_tracker.remove(deletedfiles);
        this->look_up_and_remove_storage(file_name, 1);
    }
    
    //insert new file and size into tracker
    file_tracker.set(filename, file_size);
    
    cout << "Current storage capacity is: " << this->get_size_of_storage() << endl;
    
}

long long CDN_Node::get_size_of_storage() {
    size_of_storage = 0;
    struct stat sb;
    if((dir = opendir(wd)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            stat(path_maker(ent->d_name), &sb);
            //cout << "size of file: " << sb.st_size << endl;
            size_of_storage += (long long)sb.st_size;
        }
        closedir (dir);
    } else {
        /* can not open directory */
        perror("Can't open the directory");
        return EXIT_FAILURE;
    }
        return size_of_storage;
}

char* CDN_Node::path_maker(const char* name){
    char path_file[256];
    strcpy(path_file, wd);
    strcat(path_file, "/");
    return strcat(path_file, name);
}

void CDN_Node::startListening(){
	CDNReceiver::initialize(U(CDN_ADDR), this);
}

void CDN_Node::endListening(){
	CDNReceiver::shutDown();
}

/*
 Before get the gps information, we have to convert IP address into some certain numbers that fit into
 GPS lookup table. So we have to get external IP address first. Then split and convert it into integers.
 */

void CDN_Node::get_and_set_CDN_addr() {
    string line;
    ifstream IPFile;
    int n = 0;
    int i = 0;
    int m = 0;
    string test;
    string s;
    vector <int> store(4);
    
    system("curl ipecho.net/plain > ip.txt");
    
    IPFile.open ("ip.txt");
    
    if(IPFile.is_open())
    {
        getline(IPFile,line);
        m_address.ipAddr = line; //assign ip address
        string test = line;
        
        //get the length of string
        int count = test.length();
        
        while (n < count) {
            
            //When we encounter '.', we split the string and convert it into intger
            if(line[n] == '.') {
                i = atoi(s.c_str());
                
                store[m]=i;
                
                s ="";
                m++;
                n++;
                continue;
            }
            s = s + line[n];
            n++;
            
        }
        //convert remaining part and push it into vecotr
        i = atoi(s.c_str());
        store[m] = i;
        IPFile.close();
    }
    
    //convert ip numbers for GPS information
    this->CDN_addr = (16777216 * store[0]) + (65536 * store[1]) + (256 * store[2]) + store[3];

}

/*
 Using the numbers attained from above, we can get gps information from CSV file which contain USA local latitude and longitude information.
 */

pair <double, double> CDN_Node::get_gps_info (){

    ifstream f;
    string str;
    CData data;
    vector<CData> vdata;
    
    //this path have to change.
    f.open("/home/jin/workspace/C++/CDN/CDN/USA_edit.csv");
    
    if (!f.is_open())
    {
        cout << "failed to open the file!" << endl;
    }
    
    f >> str; // omit the header
    
    while(!f.eof())
    {
        //f >> index >> ch;
        data.Load(f);
        if (f.eof()) {
            vdata.push_back(data);
            break;
        }
        vdata.push_back(data);
    }
    
    f.close();
    
    vector<CData>::iterator it;
    int n = 0;
    
    for (it=vdata.begin();it!=vdata.end();it++)
    {
        if(CDN_addr >= vdata.at(n).GetStart() && CDN_addr <= vdata.at(n).GetEnd()) {
            cdn_gps.first = vdata.at(n).GetLatitude();
            cdn_gps.second = vdata.at(n).GetLongitude();
            return cdn_gps;
        }
       n++;
    }
    
    return cdn_gps;
}





