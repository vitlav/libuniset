#include <cstdlib>
#include <string>
#include <cstring>
#include <comedilib.h>
#include <getopt.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <list>
#include "UniXML.h"

// --------------------------------------------------------------------------
using namespace std;

struct equals 
{
	int dat;
	int cal;
};
list<equals> sortedMass;


void saveXML();
void readCalibr(int fixed);
void helpPrint();
void openXML();
void dispDiagram();
void sortData(bool rise, bool cal);

static void insn_config( comedi_t* card, int subdev, int channel, lsampl_t iotype, int range, int aref );

// --------------------------------------------------------------------------
char buf[5];
char rbuf[10];

string openFileXml,saveFileXml,nodeXml;

map<int, int> massDat;

int data = 10;
int fixed;

bool sort_rise=true;
bool sort_cal=true;

string vvod;

int subdev = 0;
int chan = 0;
int range = 0;
int aref = AREF_GROUND;
bool go = true;

static struct option longopts[] = {
	{ "help", no_argument, 0, 'h' },
	{ "read", required_argument, 0, 'r' },
	{ "subdev", required_argument, 0, 's' },
	{ "aref", required_argument, 0, 'a' },
	{ "range", required_argument, 0, 'x' },
	{ "device", required_argument, 0, 'd' },
	{ "open_xml", required_argument, 0, 'o' },
	{ "save_xml", required_argument, 0, 'f' },
	{ "node", required_argument, 0, 'n' },
	{ "inc", required_argument, 0, 'i' },
	{ "cal", required_argument, 0, 'c' },
	{ NULL, 0, 0, 0 }
};

// --------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	comedi_t* card;
	char* dev = "/dev/comedi0";
	lsampl_t data = 0;
	int optindex = 0;
	int opt = 0;

	while( (opt = getopt_long(argc, argv, "hr:s:d:a:x:o:f:n:i:c:",longopts,&optindex)) != -1 ) 
	{
		switch (opt) 
		{
			case 'h':	
				printf(" -h|--help			- this message\n");
				printf("[-r|--read] chan		- read from analog channel\n");
				printf("[-s|--subdev] sub		- use subdev number sub. (Default: 0)\n");
				printf("[-d|--device] dev		- use device dev. (Default: /dev/comedi0)\n");
				printf("[-a|--aref] val			- AREF (Default: %d)\n",aref);
				printf("[-x|--range] val		- RANGE (Default: %d)\n",range);
				printf("[-o|--open_xml] filename	- filename for reading diagram\n");
				printf("[-f|--save_xml] filename	- filename for saving diagram\n");
				printf("[-n|--node] node		- nodename where diagram is\n");
				printf("[-i|--inc] type		- sorting type\n");
				printf("				  '1' for increasing data;\n");
				printf("				  '0' for decreasing data.(Default: '1')\n");
				printf("[-c|--cal] type		- sorting type\n");
				printf("				  '1' for calibrated sort;\n");
				printf("				  '0' for data sort.(Default: '1')\n");
		
			return 0;

			case 'r':	
				chan = atoi(optarg);
			break;
			
			case 'd':	
				dev = optarg;
			break;

			case 's':	
				subdev = atoi(optarg);
			break;

			case 'x':	
				range = atoi(optarg);
			break;

			case 'a':	
				aref = atoi(optarg);
			break;
			
			case 'o':
				openFileXml=optarg;	
			break;

			case 'f':
				saveFileXml=optarg;	
			break;

			case 'n':
				nodeXml=optarg;	
			break;

			case 'i':
				sort_rise=optarg=="1"?true:false;	
			break;

			case 'c':
				sort_cal=optarg=="1"?true:false;	
			break;
			
			case '?':
			default:
				printf("? argumnet\n");
				return 0;
		}	
	}

	card = comedi_open(dev);
	if( card == NULL )
	{
		comedi_perror("comedi_open error");
		exit(EXIT_FAILURE);
	}

	insn_config(card,subdev,chan,100,range,aref);

	int fd=open("/dev/stdin",O_NONBLOCK | O_RDONLY );
	helpPrint();

	if( openFileXml.length() > 1 && nodeXml.length() > 1)
	{
		openXML();
		dispDiagram();
	}
	
	while(1)
	{	
		if(comedi_data_read(card, subdev, chan, range, AREF_GROUND, &data) < 0)
		{
			fprintf(stderr, "can't read from channel %d\n",chan);
		  	exit(EXIT_FAILURE);
		}
	
//		printf("Readed from channel %02d value is %05d", chan, data);
		
		cout << "\r" <<"data: "<< setw(5) << data << "		" << flush;
		
		int temp = read(fd, &buf,sizeof(buf));
		if(temp == 1)
			readCalibr(data);
		else if( temp >1 )
		{
			switch(buf[0])
			{	
				case 'q':
				{
					string str;
					str.clear();
					cout<<"\nQuiting... Are You shure?(y/n)"<<endl;
					getline(cin,str);	
					if(  str == "y")
						return 0;
					break;	
				}
				case 'v':
				{
					string str;
					cout <<"\n	Choose the type of the sorting:\n"
						 <<"		(a) on data increase\n		(b) on data decrease\n"
						 <<"		(c) on calibrated value increase\n"
						 <<"		(d) on calibrated value decrease"<<endl;
					getline(cin,str);
					cout<<"		"<<str<<endl;
					if(str.c_str()[0]=='a')
					{							
						sort_rise=true;
						sort_cal=false;
					}	
					else if(str.c_str()[0]=='b')
					{
						sort_rise=false;
						sort_cal=false;
					}	
					else if(str.c_str()[0]=='c')
					{
						sort_rise=true;
						sort_cal=true;
					}	
					else if(str.c_str()[0]=='d')
					{
						sort_rise=false;
						sort_cal=true;
					}	

					dispDiagram();
					break;
				}
				case 'c':
				{
					string str;
					str.clear();
					cout<<"\nClearing... Are You shure?(y/n)"<<endl;
					getline(cin,str);	
					if(  str == "y")
						massDat.clear();
					break;
				}
				case 'o':
				{
					openXML();
					break;
				}
				case 's':
				{	
					saveXML();
					break;
				}
				case 'd':
				{
					dispDiagram();
					break;
				}
				case ' ':
				{
					readCalibr(data);
					break;
				}
				default:	
				{
					helpPrint();
					break;
				}	
			}
		}		
		usleep(1000000);
	}
	
	return 0;
}

// --------------------------------------------------------------------------
void insn_config( comedi_t* card, int subdev, int channel, lsampl_t iotype, int range, int aref )
{
	comedi_insn insn;
	memset(&insn,0,sizeof(insn));
	insn.insn = INSN_CONFIG;
	insn.n = 1;
	insn.data = &iotype;
	insn.subdev = subdev;
	insn.chanspec = CR_PACK(channel,range,aref);
	if( comedi_do_insn(card,&insn) < 0 )
	{
		fprintf(stderr, "can`t configure (AIO) subdev=%d channel=%d type=%d",subdev,channel,iotype);
	  	exit(EXIT_FAILURE);
	}
}

// --------------------------------------------------------------------------
void readCalibr(int fixed)
{
	cout << "	Enter calibrated value for data="<< fixed <<"\n	cal = "<<flush;
	int f=open("/dev/stdin", O_RDONLY );
	
	memset(rbuf,0,sizeof(rbuf));
	int temp = read(f, &rbuf,sizeof(rbuf));
	if (temp>1)
	{
		int s;
		if(sscanf(&rbuf[0],"%d",&s)>0)
		{
			cout<<"data: "<<fixed<<"		calibrated: "<<s<<endl;
			massDat[fixed]=s;
		}
		else
				cout << "		you must input only any digits!"<<endl;
	}	
	else
				cout << "		you must input only any digits!"<<endl;
}

// --------------------------------------------------------------------------
void saveXML()
{
	cout<<"Save as: "<<endl;
	
	cout<<"		"<<saveFileXml<<endl;
	
	string str;
	str.clear();

	getline(cin,str);	
	if(  str != "")
		saveFileXml=str;	

	FILE *fp =fopen(saveFileXml.c_str(),"w");
	
	if(!fp)
	{
		cout<<"Can not open the file "<<saveFileXml<<endl;
		return;
	}
	
	if(nodeXml.length()<1)
		nodeXml="MyCalibration";
	fprintf(fp,"<Calibration>\n	<diagram name=\"");
	fprintf(fp,nodeXml.c_str());
	fprintf(fp,"\">\n");

	sortData(sort_rise,sort_cal);
	
	list<equals>::iterator it;
		for(it = sortedMass.begin(); it != sortedMass.end(); it++)
			fprintf(fp,"		<point y=\"%d\" 	x=\"%d\" />\n",it->cal,it->dat);

	fprintf(fp,"	</diagram>\n</Calibration>\n");	
	fclose(fp);
}

// --------------------------------------------------------------------------
void helpPrint()
{
	cout << endl <<"Type commands:" << endl;
	cout << "	'q'-	exit;"<< endl;
	cout << "	' '-	enter calibrated value;" <<endl;
	cout << "	'o'-	open XML file;"<<endl;	
	cout << "	'd'-	display data;"<<endl;
	cout << "	'v'-	sort data;" <<endl;
	cout << "	'c'-	clear data;"<<endl;
	cout << "	's'-	save XML file;"<<endl<<endl;
}

// --------------------------------------------------------------------------
void openXML()
{
	for(;;)
	{
		cout<<"Open file: "<<endl;
		cout<<"		"<<openFileXml<<endl;
	
		string str;
		str.clear();

		getline(cin,str);	
		if(  str != "")
			openFileXml=str;	

		try
		{
			UniXML uxml(openFileXml);

			if( nodeXml.length() < 1 )
			{
				cout <<"Enter XML diagram node name for calibration:"<<endl;
				getline(cin,nodeXml);
				cout<<"		"<< nodeXml <<endl;
			}
	
			xmlNode* root;
	
			root = uxml.findNode(uxml.getFirstNode(),"diagram",nodeXml);
			if(!root)
			{
				cout<<"XML diagram node "<< nodeXml <<" not found !!!" <<endl;
				uxml.close();
				return;
			}
	
			UniXML_iterator it(root);
			if( !it.goChildren() )
			{
				cout<<"The diagram "<< nodeXml <<" does not consist any points"<<endl;
				uxml.close();
				return;
			}
			
			int ndat, ncal;
			for(;it;it.goNext())
			{
				ndat = atoi(it.getProp("x").c_str());
				ncal = atoi(it.getProp("y").c_str());
				massDat[ndat]=ncal;
			}
		
			uxml.close();
			return;
		}
		catch( ... )
		{
			cout<<"File "<<openFileXml<<"can not be opened"<<endl;
		}
	}
}

// --------------------------------------------------------------------------
void dispDiagram()
{
	cout.setf( ios::right, ios::adjustfield );
	cout << endl <<"================================="<< endl;
	cout << "|      data	|   calibrated	|"<<endl;
	cout << "---------------------------------"<< endl;

	sortData(sort_rise,sort_cal);
	
	list<equals>::iterator it;
		for(it = sortedMass.begin(); it != sortedMass.end(); it++)
			cout<<"|	"<<it->dat<<"	|	"<<it->cal<<"	|"<<endl;

	cout << "================================="<< endl;
	cout << sortedMass.size() <<"	"<<massDat.size()<<endl;
}

// --------------------------------------------------------------------------
void sortData(bool rise,bool cal)
{		
	sortedMass.clear();
	if(rise && cal)
	{
		list<int> temp, teqv;
		map<int,int>::iterator it;
		for(it = massDat.begin();it!=massDat.end();it++)
			temp.push_back(it->second);
	
		temp.sort();
		list<int>::iterator itl;
		list<int>::iterator ite;
		int tt = *(temp.end());
		for(itl = temp.begin(); itl != temp.end(); itl++)
		{
			if(tt == *itl)continue;
			teqv.clear();
			for(it = massDat.begin();it != massDat.end(); it++)
			{
				if(*itl==it->second)
					teqv.push_back(it->first);
			}
			teqv.sort();
			for(ite = teqv.begin();ite != teqv.end(); ite++)
			{	
				equals eq;
				eq.dat=*ite;
				eq.cal=*itl;
				sortedMass.push_back(eq);
			}
			tt=*itl;				
		}		
	
	}
	else if(!rise && cal)
	{
		list<int> temp, teqv;
		map<int,int>::iterator it;
		for(it = massDat.begin();it!=massDat.end();it++)
			temp.push_back(it->second);
	
		temp.sort();
		list<int>::iterator itl;
		list<int>::iterator ite;
		int tt = *(temp.begin());
		for(itl = temp.end(); itl != temp.begin(); --itl)
		{
			if(tt == *itl)continue;
			teqv.clear();
			for(it = massDat.begin();it != massDat.end(); it++)
			{
				if(*itl == it->second)
					teqv.push_back(it->first);
			}
			teqv.sort();
			for(ite = teqv.begin();ite != teqv.end(); ite++)
			{	
				equals eq;
				eq.dat=*ite;
				eq.cal=*itl;
				sortedMass.push_back(eq);
			}
			tt=*itl;				
		}	
	}
	else if(rise && !cal)
	{
		map<int,int>::iterator it;
		for(it = massDat.begin();it!=massDat.end();it++)
		{
			equals eq;
			eq.dat=it->first;
			eq.cal=it->second;
			sortedMass.push_back(eq);
		}	
	}
	else if(!rise && !cal)
	{
		map<int,int>::iterator it;
		for(it = --massDat.end();it!=--massDat.begin();it--)
		{
			equals eq;
			eq.dat=it->first;
			eq.cal=it->second;
			sortedMass.push_back(eq);
		}	
	}
}

// --------------------------------------------------------------------------

