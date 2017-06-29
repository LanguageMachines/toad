#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <ticcutils/StringOps.h>

using namespace std;

int main(){
  string line;
  vector<string> parts;
  if ( !getline( cin, line ) ){
    cerr << "empty input" << endl;
    exit(EXIT_FAILURE);
  }
  int size = TiCC::split_at( line, parts, "\t" );
  string word = parts[0];
  string ner = parts[1];
  string prev = "_";
  string cur = parts[2];
  string next;
  while ( getline( cin, line ) ){
    int size = TiCC::split_at( line, parts, "\t" );
    if ( size < 1 ){
      // ok, close previous group
      cout << word << "\t" << "\t" << ner << "\t" << prev << "\t" << cur << "\t_" << endl << endl;
      prev = "_";
      if ( !getline( cin, line )){
	break;
      }
      int size = TiCC::split_at( line, parts, "\t" );
      if ( size < 1 ){
	cerr << "2 empty lines in a row! " << endl;
	exit(EXIT_FAILURE);
      }
      else {
	word = parts[0];
	ner = parts[1];
	prev = cur;
	cur = parts[2];
	next = "_";
      }
    }
    else {
      next = parts[2];
      cout << word << "\t" << "\t" << ner << "\t" << prev << "\t" << cur << "\t" << next << endl;
      word = parts[0];
      ner = parts[1];
      prev = cur;
      cur = parts[2];
      next = "ERR";
    }
  }
}
