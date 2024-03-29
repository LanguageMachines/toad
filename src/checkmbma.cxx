/*
  Copyright (c) 2010 - 2024
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
#include<getopt.h>
#include<iostream>
#include<fstream>
#include<vector>
#include<set>
#include<map>
#include<string>
#include<cstdlib>
#include "timbl/TimblAPI.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/Unicode.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "frog/FrogAPI.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace	icu;

TiCC::LogStream *theErrLog = new TiCC::LogStream(cerr);

TiCC::Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/frog/nld/";
static string configFileName = configDir + "frog.cfg";

Mbma myMbma(theErrLog);

set<UnicodeString> lexicon;
set<UnicodeString> mor_lexicon;

bool isException( const string& s ){
  if ( s.size() < 2 )
    return true;
  return false;
}

void usage(){
  cerr << "checkmbma [-h] [-m] [-S<limit>]" << endl;
  cerr << "check mbma-merged.lex for inconsistencies" << endl;
  cerr << "\t -m signal unknow morphemes too. (a lot!) " << endl;
}

void check_word( const UnicodeString& _word, bool doMor ){
  UnicodeString uword = _word;
  UnicodeString ls = uword;
  ls.toLower();
  if ( uword != ls ){
    return;
  }
  myMbma.Classify( ls, "" );
  vector<pair<UnicodeString,string>> anas = myMbma.getResults(true);
  set<UnicodeString> fails;
  for ( const auto& ana : anas ){
    UnicodeString flat = flatten(ana.first);
    bool lem_found = false;
    vector<UnicodeString> mors = TiCC::split_at_first_of( flat, "[]" );
    bool first = true;
    for ( const auto& mor : mors ){
      UnicodeString mor1 = mor;
      mor1.toLower();
      if ( mor1 == uword ){
	lem_found = true;
	break;
      }
      else if ( lexicon.find(mor1) != lexicon.end() ){
	//	  cerr << "found lemma " << mor1 << endl;
	lem_found = true;
      }
      else if ( doMor
		&& mor1.length() != 1
		&& !first
		&& mor_lexicon.find(mor1) == mor_lexicon.end() ){
	//	  cerr << "NOT found mor " << mor << endl;
	fails.insert(mor1);
      }
      first = false;
    }
    if ( !lem_found ){
      using TiCC::operator<<;
      cerr << "UNK LEMMA " << _word << " - " << ana << endl;
    }
    else if ( fails.size() > 0 ){
      using TiCC::operator<<;
      cerr << "UNK MOR ";
      for ( const auto& f : fails ){
	cerr << "[" << f << "] ";
      }
      cerr << _word << " - " << ana << endl;
    }
  }
}

int main(int argc, char * const argv[] ) {
  string lexname = "mbma-merged.lex";
  string inpname ;
  bool doMor = false;
  bool testSonar = false;
  string debug;
  size_t limit = 0;
  int opt;
  while ( (opt = getopt( argc, argv, "d:hmS:t:")) != -1 ){
    switch ( opt ){
    case 'm': doMor = true; break;
    case 'd':
      debug = optarg;
      break;
    case 'S':
      testSonar = true;
      limit = std::stol( optarg );
      break;
    case 't':
      inpname = optarg;
      break;
    case 'h': usage(); return EXIT_SUCCESS; break;
    default: usage(); return EXIT_FAILURE;
    }
  }

  ifstream bron( lexname );
  if ( !bron ){
    cerr << "could not open mbma file '" << lexname << "'" << endl;
    return EXIT_FAILURE;
  }

  map<UnicodeString,size_t> test_lex;
  cout << "building a lexicon from " << lexname << endl;
  UnicodeString uline;
  while ( TiCC::getline( bron, uline ) ){
    vector<UnicodeString> parts = TiCC::split_at( uline, " " );
    if ( parts.size() < 2 ){
      cerr << "Problem in line '" << uline << "' (to short?)" << endl;
      continue;
    }
    UnicodeString word = parts[0];
    word.toLower();
    int num = (int)parts.size()-1;
    if ( word.length() != num ){
      cerr << "Problem in line '" << uline << "' (" << word.length()
	   << " letters, but got " << num << " morphemes)" << endl;
      continue;
    }
    lexicon.insert( word );
  }
  cout << "found " << lexicon.size() << " words " << endl;
  bron.close();
  bron.open( "sonar.lemmas" );
  while ( TiCC::getline(bron, uline ) ){
    if ( uline.isEmpty() ) {
      continue;
    }
    vector<UnicodeString> vec = TiCC::split( uline );
    lexicon.insert( vec[0] );
  }
  bron.close();
  cout << "added sonar lemmas, size is now: " << lexicon.size() << " words " << endl;
  bron.open( "known.lemmas" );
  while ( TiCC::getline(bron, uline ) ){
    if ( uline.isEmpty() ){
      continue;
    }
    vector<UnicodeString> vec = TiCC::split( uline );
    lexicon.insert( vec[0] );
  }
  bron.close();
  cout << "added known lemmas, size is now: " << lexicon.size() << " words " << endl;
  bron.open( "known.morphs" );
  while ( TiCC::getline(bron, uline ) ){
    if ( uline.isEmpty() ){
      continue;
    }
    vector<UnicodeString> vec = TiCC::split( uline );
    mor_lexicon.insert( vec[0] );
  }
  bron.close();
  cout << "found " << mor_lexicon.size() << " known morphemes." << endl;
  bron.close();
  if ( testSonar ){
    bron.open( "sonar.words" );
    while ( TiCC::getline(bron, uline ) ){
      if ( uline.isEmpty() ){
	continue;
      }
      vector<UnicodeString> vec = TiCC::split( uline );
      if ( vec.size() == 4 ){
	size_t freq;
	if ( !TiCC::stringTo<size_t>( TiCC::UnicodeToUTF8(vec[1]), freq ) ){
	  cerr << "illegal int in " << uline << endl;
	  continue;
	}
	if ( freq > limit ){
	  test_lex[ vec[0] ] = freq;
	}
      }
    }
    bron.close();
    cout << "read " << test_lex.size() << " test words from sonar.words" << endl;
  }

  configuration.fill( configFileName );
  if ( !debug.empty() ){
    configuration.setatt( "debug", debug, "mbma" );
  }
  myMbma.init( configuration );
  if ( testSonar ){
    cout << "checking the morphemes in sonar.lemmas " << endl;
    for ( const auto& it : test_lex ){
      check_word( it.first, doMor );
    }
  }
  else if ( !inpname.empty() ){
    bron.open( inpname );
    cout << "checking the morphemes in " << inpname << endl;
    while ( TiCC::getline(bron, uline ) ){
      vector<UnicodeString> parts = TiCC::split( uline );
      check_word( parts[0], doMor );
    }
  }
  else {
    bron.open( lexname );
    cout << "checking the morphemes in " << lexname << endl;
    while ( TiCC::getline(bron, uline ) ){
      vector<UnicodeString> parts = TiCC::split_at( uline, " " );
      check_word( parts[0], doMor );
    }
  }
  return 0;
}
