/*
  Copyright (c) 2010 - 2017
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
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <cstdlib>
#include "timbl/TimblAPI.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "frog/FrogAPI.h"
#include "frog/mblem_mod.h"

using namespace std;

int tpDebug=0;
TiCC::LogStream *theErrLog = new TiCC::LogStream(cerr);

TiCC::Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/frog/";
static string configFileName = configDir + "frog.cfg";

void usage(){
  cerr << "checkmblem [-i inputfile]" << endl;
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

bool isException( const string& s ){
  if ( s.size() < 3 )
    return true;
  return false;
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("i:","");
  try {
    opts.parse_args( argc, argv );
  }
  catch ( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage();
    return EXIT_FAILURE;
  }
  string inpname = "mblem.lex";
  opts.extract( 'i', inpname );
  ifstream bron( inpname );
  if ( !bron ){
    cerr << "could not open input file '" << inpname << "'" << endl;
    return EXIT_FAILURE;
  }

  set<string> lexicon;
  cout << "building a lexicon from " << inpname << endl;
  string line;
  while ( getline(bron, line ) ){
    vector<string> parts;
    int num = TiCC::split_at( line, parts, " " );
    if ( num != 3 ){
      cerr << "Problem in line '" << line << "' (to short?)" << endl;
      continue;
    }
    UnicodeString word = TiCC::UnicodeFromUTF8( parts[0] );
    word.toLower();
    lexicon.insert( TiCC::UnicodeToUTF8( word ) );
  }
  cout << "found " << lexicon.size() << " words " << endl;
  bron.close();
  unsigned int count = 0;
  bron.open( "sonar.lemmas" );
  while ( getline(bron, line ) ){
    if ( line.empty() )
      continue;
    ++count;
    vector<string> vec;
    TiCC::split( line, vec );
    lexicon.insert( vec[0] );
  }
  bron.close();
  cout<< "added " << count << " words from sonar.lemmas" << endl;
  count = 0;
  bron.open( "known.lemmas" );
  while ( getline(bron, line ) ){
    if ( line.empty() )
      continue;
    ++count;
    vector<string> vec;
    TiCC::split( line, vec );
    lexicon.insert( vec[0] );
  }
  cout<< "added " << count << " words from known.lemmas" << endl;
  bron.close();
  Mblem myMblem(theErrLog);
  configuration.fill( configFileName );
  myMblem.init( configuration );
  bron.open( inpname );
  cout << "checking the lemmas in " << inpname << endl;
  while ( getline(bron, line ) ){
    vector<string> parts;
    int num = TiCC::split_at( line, parts, " " );
    if ( num != 3 ){
      cerr << "Problem in line '" << line << "' (to short?)" << endl;
      continue;
    }
    string word = parts[0];
    UnicodeString us( word.c_str() );
    UnicodeString ls = us;
    ls.toLower();
    if ( us != ls ){
      // skip uppercases stuff
      continue;
    }
    myMblem.Classify( ls );
#define LONG
#ifdef LONG
    vector<pair<string,string> > res = myMblem.getResult();
    for ( auto const r : res ){
      string lem = r.first;
      lem = TiCC::lowercase(lem);
      if ( lem != word
	   && !isException( lem ) ){
	if ( lexicon.find(lem) == lexicon.end() ){
	  cerr << word << " ==> " << lem << endl;
	}
      }
    }
#endif
  }
  return 0;
}
