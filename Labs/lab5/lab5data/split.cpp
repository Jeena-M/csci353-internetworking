/*
 * g++ -g -Wall -std=c++11 -o split split.cpp
 * usage: ./split
 */

/* C++ standard include files first */
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

/* C system include files next */
/* C standard include files next */
/* your own include last - none for this program */

int main()
{
    string tmp;

    string s = "a;bb;ccc;dddd;eeeee";
    cout << "Splitting '" << s << "' using getline()." << endl;
    stringstream ss;
    ss << s;
    while (getline(ss, tmp, ';')) {
        cout << "\t" << tmp << endl;
    }

    string keyvalue = "abc: defg";
    cout << "Splitting '" << keyvalue << "' using getline()." << endl;
    stringstream ss2;
    ss2 << keyvalue;
    while (getline(ss2, tmp, ':')) {
        while (tmp[0] == ' ') tmp = tmp.substr(1);
        cout << "\t" << tmp << endl;
    }
    return 0;
}
