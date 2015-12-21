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
#include "ticcutils/Configuration.h"
#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "config.h"

using namespace std;

int debug = 0;
const int HISTORY = 20;

static Configuration config;

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

const map<string,set<string>> particles = { {"WW(vd", {"be","ge"}} };

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
	//  find out whether there may be a prefix or infix particle
	for( const auto it : particles ){
	  if ( !prefixed.isEmpty() )
	    break;
	  thisform = wordform;
	  if ( tag.indexOf(it.first.c_str()) >= 0 ){
	    // the POS tag matches, so potentially yes
	    int part_pos = -1;
	    UnicodeString part;
	    for ( const auto& p : it.second ){
	      // loop over potential particles.
	      part_pos = thisform.indexOf(p.c_str());
	      if ( part_pos != -1 ){
		part = p.c_str();
		if ( debug ){
		  cerr << "alert - " << thisform << " " << lemma << endl;
		  cerr << "matched " << part << " position: " << part_pos << endl;
		}
		UnicodeString edit = thisform;
		//
		// A bit tricky here
		// We remove the first particle
		// the last would be better (e.g 'tegemoetgekomen' )
		// but then frogs mblem module needs modification too
		// need more thinking. Are there counterexamples?
		if ( (size_t)part_pos != string::npos
		     && part_pos < thisform.length()-5 ){
		  prefixed = part;
		  edit = edit.remove( part_pos, prefixed.length() );
		  if ( debug ){
		    cerr << " simplified from " << thisform
			 << " to " << edit << " vergelijk: " << lemma << endl;
		  }
		  int ident=0;
		  while ( ( ident < edit.length() ) &&
			  ( ident < lemma.length() ) &&
			  ( edit[ident]==lemma[ident] ) ){
		    ident++;
		  }
		  if (ident<5) {
		    // so we want at least 5 characters in common between lemma and our
		    // edit. Otherwise discard.
		    if ( debug )
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
	      if ( !prefixed.isEmpty() )
		break;
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
  string timblopts;
  timblopts = config.lookUp( "timblOpts", "mblem" );
  if ( timblopts.empty() ){
    timblopts = "-a1 -w2 +vS";
  }
  cout << "Timbl: Start training " << inputfile << " with Options: "
       << timblopts << endl;
  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( inputfile );
  timbl.WriteInstanceBase( outputfile );
  cout << "Timbl: Done, stored instancebase : " << outputfile << endl;
}

int main( int argc, char * const argv[] ) {
  TiCC::CL_Options opts("T:l:e:O:c:hV","");
  opts.parse_args( argc, argv );

  string corpusname;
  string outputdir;
  string lemmaname;
  string configfile;
  string encoding = "UTF-8";

  if ( opts.extract( 'h' ) ){
    usage();
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'V' ) ){
    cerr << "VERSION: " << VERSION << endl;
    exit( EXIT_SUCCESS );
  }

  if ( !opts.extract( 'T', corpusname ) ){
    cerr << "Missing a corpus!, (-T option)" << endl;
    exit( EXIT_FAILURE );
  }
  if ( opts.extract( 'c', configfile ) ){
    if ( !config.fill( configfile ) ) {
      cerr << "unable to open:" << configfile << endl;
      exit( EXIT_FAILURE );
    }
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
  string baseName = config.lookUp( "baseName", "tagger" );
  baseName = outputdir + baseName;
  string tagDataName = baseName + ".data";
  ofstream os( tagDataName );
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
  cout << "created an inputfile for the tagger: " << tagDataName << endl;
  string p_pat = config.lookUp( "p", "tagger" );
  if ( p_pat.empty() ){
    p_pat = "dddwfWawa";
  }
  string P_pat = config.lookUp( "P", "tagger" );
  if ( P_pat.empty() ){
    P_pat = "chnppdddwFawasss";
  }
  string timblopts = config.lookUp( "timblOpts", "tagger" );
  if ( timblopts.empty() ){
    timblopts = "+vS -G0 +D K: -w1 -a1 U: -a0 -w1 -mM -k9 -dIL";
  }
  string M_opt = config.lookUp( "M", "tagger" );
  if ( M_opt.empty() ){
    M_opt = "500";
  }
  string N_opt = config.lookUp( "N", "tagger" );
  string taggercommand = "-T " + tagDataName
    + " -s " + baseName + ".settings"
    + " -p " + p_pat + " -P " + P_pat
    + " -O\""+ timblopts + "\""
    + " -M " + M_opt;
  if ( !N_opt.empty() ){
    taggercommand += " -N " + N_opt;
  }

  cout << "start tagger: " << taggercommand << endl;
  MbtAPI::GenerateTagger( taggercommand );
  cout << "finished tagger" << endl;
#endif
  // for( const auto& it : data ){
  //   for ( const auto& it2 : it.second ){
  //     cout << it.first << " " << it2.first << " " << it2.second << endl;
  //   }
  // }
  string mblemtreefile = config.lookUp( "treeFile", "mblem" );
  mblemtreefile = outputdir + mblemtreefile;
  string mblemdatafile = mblemtreefile + ".data";
  create_mblem_trainfile( data, mblemdatafile );
  train_mblem( mblemdatafile, mblemtreefile );
  return EXIT_SUCCESS;
}
