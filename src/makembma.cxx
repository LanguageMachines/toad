/*
  Copyright (c) 2006 - 2016
  ILK  Tilburg University
  CLST Radboud University

  This file is part of toad

  toad is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  toad is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  For questions and suggestions, see:
      https://github.com/LanguageMachines/toad/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/
/* makemorphodata - generate 6-1-6 instances from morph-analysis lexicon
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <string>
#include <cstdlib>
#include <getopt.h>
#include "ticcutils/StringOps.h"
#include "ticcutils/LogStream.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "frog/mbma_mod.h"

using namespace std;
#define LEFT 6
#define RIGHT 6

static Mbma myMbma(new TiCC::LogStream(cerr));

UnicodeString UnicodeStringFromS( const string& s, const string& enc = "UTF8" ){
  return UnicodeString( s.c_str(), s.length(), enc.c_str() );
}

void usage(){
  cerr << "makembma [-i inputfile] [-o outputfile]"
       << endl;
}

template< typename T >
std::ostream& operator<< ( std::ostream& os, const std::set<T>& s ){
  os << "{";
  typename std::set<T>::const_iterator it = s.begin();
  while ( it != s.end() ){
    os << *it;
    ++it;
    if ( it != s.end() )
      os << ",";
  }
  os << "}";
  return os;
}

void spitOut( ostream& os, const UnicodeString& word,
	      vector<set<string> >& morphemes ){
  for ( int i=0; i < word.length(); ++i ){
    UnicodeString out;
    // left context
    for ( int j=0; j<LEFT; j++){
      if ((i-(LEFT-j))<0)
	out += "_";
      else
	out += word[i-(LEFT-j)];
      out += ",";
    }
    // focus
    out += word[i];
    out += ",";
    // right context
    for ( int j=0; j<RIGHT; j++) {
      if ( (i+j+1) >= word.length() )
	out += "_";
      else
	out += word[i+j+1];
      out += ",";
    }
    // class
    set<string>::const_iterator it = morphemes[i].begin();
    while ( it != morphemes[i].end() ){
      out += UnicodeStringFromS( *it );
      ++it;
      if ( it != morphemes[i].end() )
	out += "|";
    }
    os << out << endl;
  }
}

int main(int argc, char * const argv[] ) {
  string inpname;
  string outname;

  int opt;
  while ( (opt = getopt( argc, argv, "i:o:")) != -1 ){
    switch ( opt ){
    case 'i': inpname = optarg; break;
    case 'o': outname = optarg; break;
    default: usage(); return EXIT_FAILURE;
    }
  }

  if ( inpname == outname ){
    cerr << "input file and outputfile cannot have the same name!" << endl;
    return EXIT_FAILURE;
  }

  ifstream bron( inpname.c_str() );
  if ( !bron ){
    cerr << "could not open input file '" << inpname << "'" << endl;
    return EXIT_FAILURE;
  }

  ostream *os;
  if ( outname.empty() )
    os = &cout;
  else {
    os = new ofstream( outname.c_str() );
    if ( !os ){
      cerr << "could not open output file '" << outname << "'" << endl;
      return EXIT_FAILURE;
    }
  }

  string line;
  vector<set<string> > morphemes;
  morphemes.resize(250);
  UnicodeString prevword;
  while ( getline(bron, line ) ){
    if ( line.empty() ){
	continue;
    }
    vector<string> parts;
    int num = TiCC::split( line, parts );
    if ( num < 2 ){
      cerr << "Problem in line '" << line << "' (to short?)" << endl;
      exit(1);
    }
    UnicodeString word = UnicodeStringFromS( parts[0] );
    if ( word.length() != num-1 ){
      cerr << "Problem in line '" << line << "' (" << word.length()
	   << " letters, but got " << num-1 << " morphemes)" << endl;
      exit(1);
    }
    parts.erase(parts.begin());
    vector<Rule *> r = myMbma.execute( word, parts );
    if ( r.empty() ){
      cerr << "problems with entry: '" << line << "'" << endl;
      continue;
    }
    if ( word != prevword ){
      if ( !prevword.isEmpty() ){
	spitOut( *os, prevword, morphemes );
      }
      prevword = word;
      for ( size_t i=0; i < morphemes.size(); ++i ){
	morphemes[i].clear();
      }
    }
    for ( size_t i=0; i < parts.size(); ++i ){
      morphemes[i].insert(parts[i]);
    }
  }
  if ( !prevword.isEmpty() ){
    spitOut( *os, prevword, morphemes );
  }
  return 0;
}
