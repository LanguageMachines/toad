/*
  Copyright (c) CLS 2015-2016

  Froggen: A Generator for Frog datafiles.

  Centre for Language Studies, Radboud University Nijmegen

  Comments and bug-reports are welcome at our issue tracker at
  https://github.com/LanguageMachines/timblserver/issues
  or by mailing
  lamasoftware (at ) science.ru.nl
*/


#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"

using namespace std;

int debug = 0;
const int HISTORY = 20;

UnicodeString UnicodeFromS( const string& s, const string& enc = "UTF8" ){
  return UnicodeString( s.c_str(), s.length(), enc.c_str() );
}

string UnicodeToUTF8( const UnicodeString& s ){
  string result;
  s.toUTF8String(result);
  return result;
}

void usage(){
  cerr << "froggen -T tagggedcorpus [-l lemmalist] [-e encoding] [-O outputdir]"
       << endl;
}

void fill_lemmas( istream& is,
		  multimap<UnicodeString, map<UnicodeString, set<UnicodeString>>>& lems,
		  const string& enc ){
  string line;
  while ( getline( is, line ) ){
    if ( line == "<utt>" )
      continue;
    vector<string> parts;
    size_t num = TiCC::split( line, parts );
    if ( num != 3 ){
      cerr << "wrong inputline (should be 3 parts)" << endl;
      cerr << "'" << line << "'" << endl;
      exit( EXIT_FAILURE );
    }
    vector<UnicodeString> uparts(3);
    uparts[0] = UnicodeFromS( parts[0], enc ); // the word
    uparts[1] = UnicodeFromS( parts[1], enc ); // the lemma
    uparts[2] = UnicodeFromS( parts[2], enc ); // the POS tag
    auto it = lems.lower_bound( uparts[0] );
    if ( it == lems.upper_bound( uparts[0] ) ){
      // so a completely new word
      it = lems.insert( make_pair( uparts[0], map<UnicodeString,set<UnicodeString>>() ) );
      it->second[uparts[1]].insert( uparts[2] );
    }
    else {
      // word seen before. But with this lemma?
      auto it2 = it->second.find( uparts[1] );
      if ( it2 == it->second.end() ){
	// so this lemma not yet done for this word
	it->second[uparts[1]].insert( uparts[2] );
      }
      else {
	it2->second.insert( uparts[2] );
      }
    }
  }
}

UnicodeString lemma_lookup( multimap<UnicodeString, map<UnicodeString, set<UnicodeString>>>& data, const UnicodeString& word, const UnicodeString& tag ){
  auto it = data.lower_bound( word );
  if ( it == data.upper_bound( word ) ){
    // word not found
    return "";
  }
  for ( ; it != data.upper_bound( word ); ++it ){
    for ( auto it2 = it->second.begin(); it2 != it->second.end(); ++it2 ){
      if ( it2->second.find(tag) != it2->second.end() ){
	// is it in the tagset?
	return it2->first;
      }
    }
  }
  return "";
}

void create_mblem_trainfile( multimap<UnicodeString, map<UnicodeString, set<UnicodeString>>>& data, const string& filename ){
  ofstream os( filename );
  if ( !os ){
    cerr << "couldn't create mblem datafile" << filename << endl;
    exit( EXIT_FAILURE );
  }
  UnicodeString outLine;
  for ( const auto& it : data ){
    UnicodeString wordform = it.first;
    UnicodeString safeInstance;
    if ( !outLine.isEmpty() ){
      string out = UnicodeToUTF8(outLine);
      out.erase( out.length()-1 );
      os << out << endl;
      outLine.remove();
    }
    UnicodeString instance;
    // format instance
    for ( int i=0; i<HISTORY; i++) {
      int j= wordform.length()-HISTORY+i;
      if (j<0)
	instance += "= ";
      else {
	UChar uc = wordform[j];
	instance += uc;
	instance += " ";
      }
    }
    if ( safeInstance.isEmpty() ){
      // first time around
      if ( debug ){
	cerr << "NEW instance " << instance << endl;
      }
      safeInstance = instance;
      outLine = instance;
    }
    else if ( instance != safeInstance ){
      // instance changed. Spit out what we have...
      if ( debug ){
	cerr << "instance changed from: " << safeInstance << endl
	     << "to " << instance << endl;
      }
      string out = UnicodeToUTF8(outLine);
      out.erase( out.length()-1 );
      os << out << endl;
      safeInstance = instance;
      outLine = instance;
    }
    for ( const auto& it2 : it.second ){
      UnicodeString lemma  = it2.first;
      for ( auto const& tag : it2.second ){
	outLine += tag;
	UnicodeString prefixed;
	UnicodeString thisform = wordform;
	/* find out whether there may be a prefix or infix */
	if ( tag.indexOf("WW(vd") >= 0 ){
	  int gepos = thisform.indexOf("ge");
	  int bepos = thisform.indexOf("be");
	  if ( gepos != -1 ||
	       bepos != -1 ) {
	    if ( debug)
	      cerr << "alert - " << thisform << " " << lemma << endl;
	    UnicodeString edit = thisform;
	    //
	    // A bit tricky here
	    // We remove the first 'ge' OR the first 'be'
	    // the last would be better (e.g 'tegemoetgekomen' )
	    // but then frogs mblem module needs modification too
	    // need more thinking. Are there counterexamples?
	    if ( (size_t)gepos != string::npos && gepos < thisform.length()-5 ){
	      prefixed = "ge";
	      edit.remove( gepos, 2 );
	    }
	    else if ( (size_t)bepos != string::npos && bepos < thisform.length()-5 ){
	      prefixed = "be";
	      edit.remove( bepos, 2 );
	    }
	    if ( debug)
	      cerr << " simplified from " << thisform << " to " << edit << endl;
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
	      thisform = edit;
	      if ( debug ){
		cerr << " edited wordform " << thisform << endl;
	      }
	    }
	  }
	}

	UnicodeString deleted;
	UnicodeString inserted;
	int ident=0;
	while ( ident < thisform.length() &&
		ident < lemma.length() &&
		thisform[ident]==lemma[ident] )
	  ident++;
	if ( ident < thisform.length() ) {
	  for ( int i=ident; i< thisform.length(); i++) {
	    deleted += thisform[i];
	  }
	}
	if ( ident< lemma.length() ) {
	  for ( int i=ident; i< lemma.length(); i++) {
	    inserted += lemma[i];
	  }
	}
	if ( debug ){
	  cerr << " word " << thisform << ", lemma " << lemma
	       << ", prefix " << prefixed
	       << ", insert " << inserted
	       << ", delete " << deleted << endl;
	}
	if ( !prefixed.isEmpty() )
	  outLine += "+P" + prefixed;
	if ( !deleted.isEmpty() )
	  outLine += "+D" + deleted;
	if ( !inserted.isEmpty() )
	  outLine += "+I" + inserted;
	outLine += "|";
      }
    }
  }
  if ( !outLine.isEmpty() ){
    string out = UnicodeToUTF8(outLine);
    out.erase( out.length()-1 );
    os << out << endl;
    outLine.remove();
  }
  cout << "created a mblem trainingsfile: " << filename << endl;
}

void train_mblem( const string& inputfile, const string& outputfile ){
  string timblopts = "-a1 -w2 +vS";
  cout << "Timbl: Start training " << inputfile << " with Options: "
       << timblopts << endl;
  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( inputfile );
  timbl.WriteInstanceBase( outputfile );
  cout << "Timbl: Done, stored instancebase : " << outputfile << endl;
}

int main( int argc, char * const argv[] ) {
  TiCC::CL_Options opts("T:l:e:O:","");
  opts.parse_args( argc, argv );

  string corpusname;
  string outputdir;
  string lemmaname;
  string encoding = "UTF-8";

  if ( !opts.extract( 'T', corpusname ) ){
    cerr << "Missing a corpus!, (-T option)" << endl;
    exit( EXIT_FAILURE );
  }
  opts.extract( 'l', lemmaname );
  opts.extract( 'O', outputdir );
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    if ( !isDir( outputdir ) && !createPath( outputdir ) ){
      cerr << "output dir not usable: " << outputdir << endl;
      exit(EXIT_FAILURE);
    }
  }
  opts.extract( 'e', encoding );

  multimap<UnicodeString,map<UnicodeString,set<UnicodeString>>> data;

  ifstream corpus( corpusname);
  if ( !corpus ){
    cerr << "could not open corpus file '" << corpusname << "'" << endl;
    return EXIT_FAILURE;
  }
  //#define CONVERT
#ifndef CONVERT
  cout << "start reading lemmas from: " << corpusname << endl;
  fill_lemmas( corpus, data, encoding );
#endif
  if ( !lemmaname.empty() ){
    cout << "start reading extra lemmas from: " << lemmaname << endl;
    ifstream is( lemmaname);
    if ( !is ){
      cerr << "could not open lemma list '" << lemmaname << "'" << endl;
      return EXIT_FAILURE;
    }
    fill_lemmas( is, data, encoding );
  }
#ifdef CONVERT
  ofstream os( "corpus.frog" );
  string line;
  while ( getline( corpus, line ) ){
    if ( line == "<utt>" ){
      os << line << endl;
    }
    else {
      vector<string> parts;
      size_t num = TiCC::split( line, parts );
      if ( num == 2 ){
	UnicodeString word = UnicodeFromS( parts[0] );
	UnicodeString tag = UnicodeFromS( parts[1] );
	UnicodeString lemma = lemma_lookup( data, word, tag );
	if ( lemma.length() != 0 ){
	  os << parts[0] << "\t" << UnicodeToUTF8(lemma)
	     << "\t" << parts[1] << endl;
	}
	else {
	  os << parts[0] << "\t" << parts[0] << "\t" << parts[1] << endl;
	}
      }
    }
  }
#else
  ofstream os( "mbt.tagged" );
  string line;
  corpus.clear();
  corpus.seekg(0);
  while ( getline( corpus, line ) ){
    if ( line == "<utt>" ){
      os << line << endl;
    }
    else {
      vector<string> parts;
      size_t num = TiCC::split( line, parts );
      if ( num == 3 ){
	os << parts[0] << "\t" << parts[2] << endl;
      }
      else {
	cerr << "invalid input line: " << line << endl;
	exit( EXIT_FAILURE );
      }
    }
  }
  cout << "created an inputfile for the tagger: mbt.tagged" << endl;
  string taggercommand = "-T mbt.tagged -s " + outputdir + "mbt.settings"
    + " -p dddwfWawa -P chnppdddwFawasss"
    + " -O\"+vS -G0 +D K: -w1 -a1 U: -a0 -w1 -mM -k9 -dIL\""
    + " -M500";
  cout << "start tagger: " << taggercommand << endl;
  MbtAPI::GenerateTagger( taggercommand );
  cout << "finished tagger" << endl;
#endif
  // for( const auto& it : data ){
  //   for ( const auto& it2 : it.second ){
  //     cout << it.first << " " << it2.first << " " << it2.second << endl;
  //   }
  // }
  string mblemdatafilename = "mblem.data";
  string mblemoutputfilename = "mblem.tree";
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    mblemdatafilename = outputdir + mblemdatafilename;
    mblemoutputfilename = outputdir + mblemoutputfilename;
  }
  create_mblem_trainfile( data, mblemdatafilename );
  train_mblem( mblemdatafilename, mblemoutputfilename );
  return EXIT_SUCCESS;
}
