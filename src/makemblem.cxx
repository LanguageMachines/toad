/*
  Copyright (c) 2006 - 2023
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

/* make_lemmatizer_instances - from cgn_wordforms_with_lemmas_postags,
   build an instance base of lemmatizer instances, for the CGN
   memory-based lemmatizer MBLEM */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include "ticcutils/Unicode.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"

#define HISTORY    20
#define DEBUG       0

using namespace std;
using namespace	icu;

size_t split( const string& src, vector<string>& results,
	      const string& seps ){
  // split a string into substrings, using the characters in seps
  // as seperators
  // silently skip empty entries (e.g. when two or more seperators co-incide)
  results.clear();
  string::size_type e, s = src.find_first_not_of( seps );
  string res;
  while ( s != string::npos ){
    e = src.find_first_of( seps, s );
    if ( e == string::npos ){
      res = src.substr( s );
      s = e;
    }
    else {
      res = src.substr( s, e - s );
      s = src.find_first_not_of( seps, e );
    }
    if ( !res.empty() )
      results.push_back( res );
  }
  return results.size();
}

void usage(){
  cerr << "makemblem [-i inputfile] [-o outputfile] [-t translationsfile]"
       << endl;
  cerr << "defaults: -i mblem.lex -o mblem.data" << endl;
  cerr << "omitting -t makes us skip the translations step" << endl;
}

int main( int argc, char * const argv[] ) {
  string inpname = "mblem.lex";
  string transname;
  string outname = "mblem.data";
  int opt;
  bool I_AM_SURE = false;
  while ( (opt = getopt( argc, argv, "o:i:t:Y")) != -1 ){
    switch ( opt ){
    case 'i': inpname = optarg; break;
    case 'o': outname = optarg; break;
    case 't': transname = optarg; break;
    case 'Y': I_AM_SURE = true; break;
    default: usage(); return EXIT_FAILURE;
    }
  }

  if ( !I_AM_SURE ){
    cerr << "this is a very old program. use 'froggen' instead, please" << endl;
    cerr << "or add -Y to the commandline, if you are really really sure" << endl;
    return EXIT_FAILURE;
  }
  if ( inpname == outname ){
    cerr << "input file and outputfile cannot have the same name!" << endl;
    return EXIT_FAILURE;
  }

  bool doTrans = !transname.empty();

  ifstream tf;
  if ( doTrans ) {
    tf.open( transname );
    if ( !tf ){
      cerr << "could not open translations file '" << transname << "'" << endl;
      return EXIT_FAILURE;
    }
  }
  ifstream bron( inpname );
  if ( !bron ){
    cerr << "could not open input file '" << inpname << "'" << endl;
    return EXIT_FAILURE;
  }
  ostream *os;
  if ( outname.empty() )
    os = &cout;
  else {
    os = new ofstream( outname );
    if ( !os ){
      cerr << "could not open output file '" << outname << "'" << endl;
      return EXIT_FAILURE;
    }
  }
  cerr << "creating " << outname << " from " << inpname;
  if ( doTrans ){
    cerr << " using translations from " << transname << endl;
  }
  else {
    cerr << " without tag translations " << endl;
  }
  vector<UnicodeString> classes;
  vector<UnicodeString> classcodes;
  UnicodeString wordform;
  UnicodeString lemma;
  UnicodeString tag;
  static TiCC::UnicodeNormalizer nfc_normalizer;

  if ( doTrans ){
    UnicodeString line;
    while ( TiCC::getline( tf, line ) ){
      line = nfc_normalizer.normalize( line );
      vector<UnicodeString> parts = TiCC::split_at_first_of( line, " \t" );
      int num = parts.size();
      if ( num == 2 ){
	classes.push_back( parts[0] );
	classcodes.push_back( parts[1] );
      }
      else {
	cerr << "split failed " << num << " parts in " << line << endl;
	exit(1);
      }
    }
  }

  UnicodeString lastinstance;
  UnicodeString outLine;
  UnicodeString uline;
  while ( TiCC::getline( bron, uline ) ){
    uline = nfc_normalizer.normalize( uline );
    vector<UnicodeString> parts = TiCC::split_at_first_of( uline, " \t" );
    int num = parts.size();
    if ( num == 3 ){
      wordform = parts[0];
      lemma = parts[1];
      tag = parts[2];
    }
    else {
      cerr << "couldn't split in 3 " << uline << endl;
    }
    bool debug = DEBUG; // || (wordform == "tegemoetgekomen");
    //      = (wordform == "aangeslagen");

    if ( debug ){
      cerr << "start with wordform " << wordform << endl;
    }
    UnicodeString memwordform = wordform;
    UnicodeString prefixed;
    /* find out whether there may be a prefix or infix */
    if ( tag.indexOf("WW(vd") >= 0 ){
      int gepos = wordform.indexOf("ge");
      int bepos = wordform.indexOf("be");
      if ( gepos != -1 ||
	   bepos != -1 ) {
	if ( debug)
	  cerr << "alert - " << wordform << " " << lemma << endl;
	UnicodeString edit = wordform;
	//
	// A bit tricky here
	// We remove the first 'ge' OR the first 'be'
	// the last would be better (e.g 'tegemoetgekomen' )
	// but then frogs mblem module needs modification too
	// need more thinking. Are there counterexamples?
	if ( (size_t)gepos != string::npos && gepos <  wordform.length()-5 ){
	  prefixed = "ge";
	  edit.remove( gepos, 2 );
	}
	else if ( (size_t)bepos != string::npos && bepos <  wordform.length()-5 ){
	  prefixed = "be";
	  edit.remove( bepos, 2 );
	}
	if ( debug)
	  cerr << " simplified from " << wordform << " to " << edit << endl;
	int ident=0;
	while ( ( ident< edit.length() )&&
		( ident< lemma.length() ) &&
		( edit[ident]==lemma[ident] ) )
	  ident++;
	if (ident<5) {
	  // so we want at least 5 characters in common between lemma and our
	  // edit. Otherwise discard.
	  if (debug)
	    cerr << " must be a fake!" << endl;
	  prefixed = "";
	}
	else {
	  wordform = edit;
	  if ( debug ){
	    cerr << " edited wordform " << wordform << endl;
	  }
	}
      }
    }
    UnicodeString deleted;
    UnicodeString inserted;
    int ident=0;
    while ( ident < wordform.length() &&
	    ident < lemma.length() &&
	    wordform[ident]==lemma[ident] ){
      ident++;
    }
    if ( ident < wordform.length() ) {
      for ( int i=ident; i< wordform.length(); i++) {
	deleted += wordform[i];
      }
    }
    if ( ident< lemma.length() ) {
      for ( int i=ident; i< lemma.length(); i++) {
	inserted += lemma[i];
      }
    }
    if ( debug ){
      cerr << " word " << wordform << ", lemma " << lemma
	   << ", prefix " << prefixed
	   << ", insert " << inserted
	   << ", delete " << deleted << endl;
    }
    UnicodeString instance;
    // print instance
    for ( int i=0; i<HISTORY; i++) {
      int j= memwordform.length()-HISTORY+i;
      if (j<0)
	instance += "= ";
      else {
	UChar uc = memwordform[j];
	instance += uc;
	instance += " ";
      }
    }
    if ( instance != lastinstance ){
      if ( debug ){
	cerr << "instance changed from " << lastinstance << endl
	     << "to " << instance << endl;
      }
      if ( !outLine.isEmpty() ){
	*os << outLine << endl;
      }
      outLine.remove();
      outLine += instance;
      lastinstance = instance;
    }
    else {
      outLine += "|";
    }
    if ( doTrans ){
      size_t j = 0;
      while ( j < classes.size() && ( tag != classes[j] ) ) { ++j; };
      if ( j< classes.size() ){
	outLine += UnicodeString( classcodes[j] );
      }
      else {
	outLine += "?";
	cerr << "UNKNOWN TAG: " << tag << endl;
      }
    }
    else {
      outLine += tag;
    }
    if ( !prefixed.isEmpty() ){
      outLine += "+P" + prefixed;
    }
    if ( !deleted.isEmpty() ){
      outLine += "+D" + deleted;
    }
    if ( !inserted.isEmpty() ){
      outLine += "+I" + inserted;
    }
  }
  if ( !outLine.isEmpty() ){
    *os << outLine << endl;
  }
}
