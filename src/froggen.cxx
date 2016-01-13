/*
  Copyright (c) 2015 - 2016
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
#include <map>
#include <set>
#include <string>
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Configuration.h"
#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "config.h"

using namespace std;
using namespace TiCC;

int debug = 0;
const int HISTORY = 20;

// some defaults (for Dutch)
const string dutch_partics = "[WW(vd/be] [WW(vd/ge]";
const string dutch_p_pat = "dddwfWawa";
const string dutch_P_pat = "chnppdddwFawasss";
const string dutch_timblopts = "+vS -G0 +D K: -w1 -a1 U: -a0 -w1 -mM -k9 -dIL";
const string dutch_M_opt = "500";
const string dutch_lemma_timbl_opts = "-a1 -w2 +vS";

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
  cerr << "froggen -T tagggedcorpus [-l lemmalist] [-c configfile] [-e encoding] [-O outputdir]"
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

void create_tagger( const string& base_name, const string& corpus_name ){
  ifstream corpus( corpus_name );
  string tag_data_name = base_name + ".data";
  ofstream os( tag_data_name );
  string line;
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
  cout << "created an inputfile for the tagger: " << tag_data_name << endl;
  string p_pat = config.lookUp( "p", "tagger" );
  if ( p_pat.empty() ){
    p_pat = dutch_p_pat;
  }
  string P_pat = config.lookUp( "P", "tagger" );
  if ( P_pat.empty() ){
    P_pat = dutch_P_pat;
  }
  string timblopts = config.lookUp( "timblOpts", "tagger" );
  if ( timblopts.empty() ){
    timblopts = dutch_timblopts;
  }
  string M_opt = config.lookUp( "M", "tagger" );
  if ( M_opt.empty() ){
    M_opt = dutch_M_opt;
  }
  string N_opt = config.lookUp( "n", "tagger" );
  string taggercommand = "-T " + tag_data_name
    + " -s " + base_name + ".settings"
    + " -p " + p_pat + " -P " + P_pat
    + " -O\""+ timblopts + "\""
    + " -M " + M_opt;
  if ( !N_opt.empty() ){
    taggercommand += " -n " + N_opt;
  }
  taggercommand += " -DLogSilent"; // shut up
  cout << "start tagger: " << taggercommand << endl;
  cout << "this may take several minutes, depending on the corpus size."
       << endl;
  MbtAPI::GenerateTagger( taggercommand );
  cout << "finished tagger" << endl;
}

map<string,set<string>> particles;
void fill_particles( const string& line ){
  cout << "start filling particle info from " << line << endl;
  vector<string> parts;
  TiCC::split_at_first_of( line, parts, "[] " );
  for ( const auto& part : parts ){
    vector<string> v;
    if ( 2 != TiCC::split_at( part, v, "/" ) ){
      cerr << "error parsing particles line: " << line << endl;
      cerr << "at : " << part << endl;
      exit( EXIT_FAILURE );
    }
    particles[v[0]].insert( v[1] );
  }
}

void create_mblem_trainfile( const multimap<UnicodeString, map<UnicodeString, set<UnicodeString>>>& data,
			     const string& filename ){
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
  string timblopts = config.lookUp( "timblOpts", "mblem" );
  if ( timblopts.empty() ){
    timblopts = dutch_lemma_timbl_opts;
  }
  cout << "Timbl: Start training " << inputfile << " with Options: "
       << timblopts << endl;
  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( inputfile );
  timbl.WriteInstanceBase( outputfile );
  cout << "Timbl: Done, stored instancebase : " << outputfile << endl;
}

void create_lemmatizer( const multimap<UnicodeString,map<UnicodeString,set<UnicodeString>>>& data, const string& mblem_tree_file ){
  string mblem_data_file = mblem_tree_file + ".data";
  create_mblem_trainfile( data, mblem_data_file );
  train_mblem( mblem_data_file, mblem_tree_file );
}

void check_data( Tokenizer::TokenizerClass *tokenizer,
		 const multimap<UnicodeString,map<UnicodeString,set<UnicodeString>>>& data ){
  for ( const auto& word : data ){
    int num = tokenizer->tokenizeLine( word.first );
    if ( num != 1 ){
      cerr << "the provided tokenizer doesn't handle '" << word.first
	   << "' well (splits it into " << num << " parts.)" << endl;
      vector<string> v = tokenizer->getSentences();
      cerr << "[";
      for ( const auto& w : v ){
	cerr << w << " ";
      }
      cerr << "]" << endl;
    }
    tokenizer->reset();
  }
}
void create_frog_cfg( const string& frog_cfg,
		      const string& mbt_settings,
		      const string& mblem_tree,
		      const string& tokfile ){
  ofstream os( frog_cfg );
  if ( !tokfile.empty() ) {
    os << "[[tokenizer]]" << endl;
    os << "rulesFile=\"./" << tokfile << "\"" << endl;
  }
  os << "[[tagger]]" << endl;
  os << "settings=" << mbt_settings << endl;
  os << "set=\"http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn\"" << endl;
  os << endl;
  os << "[[mblem]]" << endl;
  os << "treeFile=" << mblem_tree << endl;
  os << "timblOpts=\"-a1 -w2\"" << endl;
  os << "set=\"http://ilk.uvt.nl/folia/sets/frog-mblem-nl\"" << endl;
  os << endl;
  cout << "stored a frog configfile template: " << frog_cfg << endl;
}

int main( int argc, char * const argv[] ) {
  TiCC::CL_Options opts("t:T:l:e:O:c:hV","");
  opts.parse_args( argc, argv );

  string corpusname;
  string outputdir;
  string lemma_name;
  string tokfile;
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
  if ( !isFile( corpusname ) ){
    cerr << "unable to find the corpus: " << corpusname << endl;
    exit( EXIT_FAILURE );
  }
  if ( opts.extract( 'c', configfile ) ){
    if ( !config.fill( configfile ) ) {
      cerr << "unable to open:" << configfile << endl;
      exit( EXIT_FAILURE );
    }
    cout << "using configuration: " << configfile << endl;
  }
  opts.extract( 'l', lemma_name );
  if ( !lemma_name.empty() ){
    if ( !isFile(lemma_name) ){
      cerr << "unable to find: '" << lemma_name << "'" << endl;
      return EXIT_FAILURE;
    }
  }
  opts.extract( 'O', outputdir );
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    if ( !isDir( outputdir ) && !createPath( outputdir ) ){
      cerr << "output dir not usable: " << outputdir << endl;
      exit(EXIT_FAILURE);
    }
  }
  opts.extract( 't', tokfile );
  Tokenizer::TokenizerClass *tokenizer = 0;
  if ( !tokfile.empty() ) {
    if ( !isFile(tokfile) ){
      cerr << "unable to find: '" << tokfile << "'" << endl;
      exit( EXIT_FAILURE );
    }
    if ( !outputdir.empty() ){
      string outname = outputdir + tokfile;
      ofstream os( outname );
      ifstream is( tokfile );
      os << is.rdbuf();
      tokfile = outname;
    }
    tokenizer = new Tokenizer::TokenizerClass();
    tokenizer->init( tokfile );
  }
  opts.extract( 'e', encoding );
  string mblem_particles = config.lookUp( "particles", "mblem" );
  if ( !mblem_particles.empty() ){
    fill_particles( mblem_particles );
  }
  else
    fill_particles( dutch_partics );

  multimap<UnicodeString,map<UnicodeString,set<UnicodeString>>> data;

  cout << "start reading lemmas from the corpus: " << corpusname << endl;
  ifstream corpus( corpusname);
  fill_lemmas( corpus, data, encoding );
  cout << "done" << endl;
  if ( !lemma_name.empty() ){
    cout << "start reading extra lemmas from: " << lemma_name << endl;
    ifstream is( lemma_name);
    fill_lemmas( is, data, encoding );
    cout << "done" << endl;
  }
  string tag_base_name = config.lookUp( "baseName", "tagger" );
  if ( tag_base_name.empty() ){
    tag_base_name = "froggen";
  }
  string tag_full_name = outputdir + tag_base_name;
  string mblem_base_name = config.lookUp( "treeFile", "mblem" );
  if ( mblem_base_name.empty() ){
    mblem_base_name = "froggen.tree";
  }
  string mblem_full_name = outputdir + mblem_base_name;
  string frog_cfg = outputdir + "frog.cfg.template";
  if ( tokenizer ){
    check_data( tokenizer, data );
  }
  create_tagger( tag_full_name, corpusname );
  create_lemmatizer( data, mblem_full_name );
  create_frog_cfg( frog_cfg, tag_base_name + ".settings" ,
		   mblem_base_name, tokfile );
  return EXIT_SUCCESS;
}
