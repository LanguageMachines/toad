/*
  Copyright (c) 2015 - 2024
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

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/Unicode.h"
#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "config.h"

using namespace std;
using namespace	icu;
using namespace TiCC;
using TiCC::operator<<;

int debug = 0;
const int HISTORY = 20;
bool lemma_file_only = false;
string output_dir="";
string temp_dir="/tmp/froggen";
string encoding="UTF-8";
static Configuration use_config;
static Configuration default_config;

void set_default_config(){
  // tagger defaults
  default_config.setatt( "settings", "Frog.mbt.1.0.settings", "tagger" );
  default_config.setatt( "p", "dddwfWawa", "tagger" );
  default_config.setatt( "P", "chnppdddwFawasss", "tagger" );
  default_config.setatt( "n", "1", "tagger" );
  default_config.setatt( "M", "500", "tagger" );
  default_config.setatt( "%", "5", "tagger" );
  default_config.setatt( "timblOpts",
			 "+vS -G0 +D K: -w1 -a1 U: -a0 -w1 -mM -k9 -dIL",
			 "tagger" );
  default_config.setatt( "set",
			 "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn",
			 "tagger" );
  default_config.setatt( "subsets_file", "ignore", "tagger" );
  default_config.setatt( "constraints_file", "ignore", "tagger" );
  default_config.setatt( "token_trans_file", "ignore", "tagger" );
  // lemmatizer defaults
  default_config.setatt( "set",
			 "http://ilk.uvt.nl/folia/sets/frog-mblem-nl",
			 "mblem" );
  default_config.setatt( "particles", "[WW(vd/be] [WW(vd/ge]", "mblem" );
  default_config.setatt( "timblOpts", "-a1 -w2 +vS", "mblem" );
  // tokenizer default
  default_config.setatt( "rulesFile", "tokconfig-nld", "tokenizer" );
  default_config.setatt( "configDir",
  			 string(SYSCONF_PATH) + "/ucto/",
  			 "tokenizer" );
}

class setting_error: public std::runtime_error {
public:
  setting_error( const string& key, const string& mod ):
    std::runtime_error( "missing key: '" + key + "' for module: '" + mod + "'" )
  {};
};

void usage( const string& name ){
  cerr << name << " -T taggedcorpus [-l lemmalist] [-c configfile] [-e encoding] [-O outputdir]"
       << endl;
  cerr << name << " trains a Tagger AND a Lemmatizer on the 'taggedcorpus', " << endl
       << "optionally the lemmatizer is also trained on 'lemmalist'.\n"
       << endl;
  cerr << "  the lemmalist MUST contain tagged words in the format: " << endl
       << "\t Word-1<tab>Lemma-1<tab>POS-tag-1" << endl
       << "\t  ..." << endl
       << "\t Word-n<tab>Lemma-n<tab>POS-tag-n" << endl;
  cerr << "  The corpus format may be:" << endl
       << "\t Word-1<tab>Lemma-1<tab>POS-tag-1" << endl
       << "\t  ..." << endl
       << "\t Word-n<tab>Lemma-n<tab>POS-tag-n" << endl
       << "\t <utt>" << endl
       << "  OR: " << endl
       << "\t Word-1<tab>POS-tag-1" << endl
       << "\t  ..." << endl
       << "\t Word-n<tab>POS-tag-n" << endl
       << "\t <utt>" << endl
       << "  With <utt> markers, to separate sentences. (use --eos to change)" << endl;
  cerr << "--eos 'mark' use 'mark' to seperate sentences. Default '<utt>'" << endl
       << "\t use 'EL' to use an empty line as separator." << endl;
  cerr << "-c 'config' an optional configfile. Use only to override the system defaults" << endl
       << "\t for the Tagger, the Lemmatizer and the Tokenizer" << endl;
  cerr << "-O 'outputdir' Store all files in 'outputdir' (Higly recommended)" << endl;
  cerr << "-e 'encoding' Normally we handle UTF-8, but other encodings are supported." << endl;
  cerr << "\t WARNING: This encoding is used for ALL datafiles!" << endl;
  cerr << "\t\t Be sure to use the same encoding for the Tagged Corpus and the lemma file." << endl;
  cerr << "\t\t The results will ALWAYS be stored in UTF-8 (NFC normalized)" << endl;
  cerr << "-t 'tokenizerfile' An ucto style rulesfile can be specified here." << endl
       << "\t It must include a full path!" << endl;
  cerr << "--postags 'file'. Read POS tags labels, from 'file' and use those" <<endl;
  cerr << "\t to validate." << endl;
  cerr << "--CGN assume CGN tags as used in the Dutch Frog" << endl;
  cerr << "\t This will add some extra files to the configuration" << endl;
  cerr << "--lemma-out 'filename' Output a lemma file, in the current directory!" << endl
       << "\t merging lemmas from the tagged corpus and the separate lemmalist" << endl
       << "\t This list is again in the right format for training." << endl;
  cerr << "--temp-dir 'dirname' The directory to store teporary files. "
       << "(default: " << temp_dir << " )" << endl;
  cerr << "-h or --help These messages." << endl;
  cerr << "-v or --version Give version info." << endl;
}

void fill_lemmas( istream& is,
		  multimap<UnicodeString, map<UnicodeString, map<UnicodeString,size_t>>>& lems,
		  const set<UnicodeString>& pos_tags,

		  const UnicodeString& eos_mark ){
  size_t line_count = 0;
  size_t eos_count = 0;
  int invalid_pos_count = 0;
  int count_2 = 0;
  UnicodeString line;
  while ( TiCC::getline( is, line, encoding ) ){
    line_count++;
    if ( line.isEmpty() ){
      continue;
    }
    if ( line == eos_mark ){
      eos_count++;
      continue;
    }
    vector<UnicodeString> parts = TiCC::split_at( line, "\t" );
    if ( parts.size() == 2 ){
      // 2 word entry, fine. Count them
      if ( ++count_2 == 4 ){
	if (line_count - eos_count == 4 ){
	  // after the 4 lines with 2 entries have past, we assume it's a 2
	  // column file, probably a corpus
	  return;
	}
	else {
	  // so is seems mixes 2 and 3 columns. getting crazy...
	  cerr << "wrong inputline on line " << line_count << " (confused)" << endl;
	  exit( EXIT_FAILURE );
	}
      }
      continue; // try some more lines
    }
    else if ( parts.size() != 3 ){
      cerr << "wrong inputline on line " << line_count << " (should be 3 parts)" << endl;
      cerr << "'" << line << "'" << endl;
      exit( EXIT_FAILURE );
    }
    // we have a 3-parts entry, which can be processed
    if ( !pos_tags.empty() ){
      if ( pos_tags.find( parts[2] ) == pos_tags.end() ){
	cerr << "Warning, unknown POS tag: " << parts[2] << " in line "
	     << line_count << " '" << line << "'" << endl;
	if ( ++invalid_pos_count > 10 ){
	  cerr << "more than 10 invalid POS tags. Please fix your data"
	       << endl;
	  exit( EXIT_FAILURE );
	}
      }
    }
    UnicodeString uword = TiCC::utrim(parts[0]); // the word
    UnicodeString ulemma = TiCC::utrim(parts[1]); // the lemma
    UnicodeString utag = TiCC::utrim(parts[2]); // the POS tag
    auto it = lems.lower_bound( uword );
    if ( it == lems.upper_bound( uword ) ){
      // so a completely new word
      it = lems.insert( make_pair( uword, map<UnicodeString,map<UnicodeString,size_t>>() ) );
      ++it->second[ulemma][utag];
    }
    else {
      // word seen before. But with this lemma?
      auto it2 = it->second.find( ulemma );
      if ( it2 == it->second.end() ){
	// so this lemma not yet done for this word
	++it->second[ulemma][utag];
      }
      else {
	++it2->second[utag];
      }
    }
  }
}

void write_lemmas( ostream& os,
		   const multimap<UnicodeString, map<UnicodeString, map<UnicodeString,size_t>>>& lems ){
  for ( const auto& it1 : lems ){
    UnicodeString word = it1.first;
    for ( const auto& it2 : it1.second ){
      UnicodeString lemma = it2.first;
      for( const auto& it3 : it2.second ){
	UnicodeString pos = it3.first;
	os << word << "\t" << lemma << "\t" << pos << endl;
      }
    }
  }
}

void create_tagger( const Configuration& config,
		    const string& base_name,
		    const string& corpus_name,
		    const set<UnicodeString>& pos_tags,
		    const UnicodeString& eos_mark ){
  cout << "create a tagger from: " << corpus_name << endl;
  ifstream corpus( corpus_name );
  string tag_data_name = temp_dir + base_name + ".data";
  ofstream os( tag_data_name );
  size_t line_count = 0;
  UnicodeString line;
  while ( TiCC::getline( corpus, line, encoding ) ){
    ++line_count;
    if ( ( line.isEmpty() && eos_mark == "EL" )
	 || line == eos_mark ){
      os << line << endl;
    }
    else {
      vector<UnicodeString> parts = TiCC::split_at( line, "\t" );
      UnicodeString word;
      UnicodeString pos;
      if ( parts.size() == 2 ){
	word = parts[0];
	pos = parts[1];
      }
      else if ( parts.size() == 3 ){
	word = parts[0];
	pos = parts[2];
      }
      else {
	cerr << "invalid input line (" << line_count << "): '" << line
	     << "'" << endl;
	exit( EXIT_FAILURE );
      }
      if ( !pos_tags.empty() ){
	if ( pos_tags.find( pos ) == pos_tags.end() ){
	  cerr << "Warning, unknown POS tag: " << pos << " in line " << line_count
	       << " '" << line << "'" << endl;
	  //	  exit( EXIT_FAILURE );
	}
      }
      os << word << "\t" << pos << endl;
    }
  }
  cout << "created an inputfile for the tagger: " << tag_data_name << endl;
  string p_pat = config.lookUp( "p", "tagger" );
  string P_pat = config.lookUp( "P", "tagger" );
  string timblopts = config.lookUp( "timblOpts", "tagger" );
  string M_opt = config.lookUp( "M", "tagger" );
  string n_opt = config.lookUp( "n", "tagger" );
  string taggercommand = "-T " + tag_data_name
    + " -s " + output_dir + base_name + ".settings"
    + " -p " + p_pat + " -P " + P_pat
    + " -O\""+ timblopts + "\""
    + " -M " + M_opt
    + " -n " + n_opt;
  //  taggercommand += " -DLogSilent --tabbed"; // shut up AND tel MBT to only use tabs as separators. Needs recent mbt.
  taggercommand += " -DLogSilent"; // shut up
  cout << "start tagger: " << taggercommand << endl;
  cout << "this may take several minutes, depending on the corpus size."
       << endl;
  MbtAPI::GenerateTagger( taggercommand );
  cout << "finished creating tagger" << endl;
}

map<UnicodeString,set<UnicodeString>> fill_particles( const string& line ){
  map<UnicodeString,set<UnicodeString>> result;
  cout << "start filling particle info from " << line << endl;
  UnicodeString uline = TiCC::UnicodeFromUTF8(line);
  vector<UnicodeString> parts = TiCC::split_at_first_of( uline, "[] " );
  for ( const auto& part : parts ){
    vector<UnicodeString> v = TiCC::split_at( part, "/" );
    if ( v.size() != 2 ){
      cerr << "error parsing particles line: " << line << endl;
      cerr << "at : " << part << endl;
      exit( EXIT_FAILURE );
    }
    result[v[0]].insert( v[1] );
  }
  return result;
}

set<UnicodeString> fill_postags( const string& pos_tags_file ){
  set<UnicodeString> result;
  if ( !pos_tags_file.empty() ){
    cout << "reading valid POS tags from file: '" << pos_tags_file
	 << "'" << endl;
    ifstream is( pos_tags_file );
    UnicodeString line;
    size_t count = 0;
    while ( TiCC::getline( is, line, encoding ) ){
      ++count;
      if ( line.isEmpty() ){
	continue;
      }
      if ( line[0] == '#' ){
	// comment
	continue;
      }
      vector<UnicodeString> v = TiCC::split( line );
      if ( v.size() > 1 ){
	result.insert( v[1] );
      }
      else {
	cerr << "invalid line (" << count << ") in '" << pos_tags_file
	     << "'" << endl;
	cerr << "expected at least two words, with a POS tag as second" << endl;
	cerr << "like: '[T105] N(soort,ev,dim,gen) vadertjes pijp'" << endl;
	cerr << "but found: '" << line << "'" << endl;
	exit( EXIT_FAILURE );
      }
    }
    cout << "\tfound " << result.size() << " tags." << endl;
  }
  return result;
}

void create_mblem_trainfile( const multimap<UnicodeString, map<UnicodeString, map<UnicodeString, size_t>>>& data,
			     const map<UnicodeString,set<UnicodeString>>& particles,
			     const string& _filename ){
  string filename = temp_dir + _filename;
  ofstream os( filename );
  if ( !os ){
    cerr << "couldn't create mblem datafile: " << filename << endl;
    exit( EXIT_FAILURE );
  }
  UnicodeString outLine;
  for ( const auto& data_it : data ){
    UnicodeString wordform = data_it.first;
    UnicodeString safeInstance;
    if ( !outLine.isEmpty() ){
      string out = UnicodeToUTF8(outLine);
      out.erase( out.length()-1 ); // remove the final '|'
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
    multimap<size_t, multimap<UnicodeString,UnicodeString>,std::greater<size_t>> sorted;
    for ( const auto& it2 : data_it.second ){
      for ( const auto& it3: it2.second ){
	multimap<UnicodeString,UnicodeString> mm;
	mm.insert(make_pair(it3.first,it2.first));
	sorted.insert(make_pair(it3.second,mm));
      }
    }
    if ( debug ){
      cerr << "sorted: " << endl;
      for ( const auto& it : sorted ){
	cerr << it.second << " (" << it.first << " )" << endl;
      }
    }
    for ( const auto& it2 : sorted ){
      for( const auto& it3 : it2.second ){
	UnicodeString tag = it3.first;
	UnicodeString lemma  = it3.second;
	if ( debug ){
	  cerr << "LEMMA = " << lemma << endl;
	  cerr << "tag = " << tag << endl;
	}
	outLine += tag;
	UnicodeString prefixed;
	UnicodeString thisform = wordform;
	//  find out whether there may be a prefix or infix particle
	for( const auto& it : particles ){
	  if ( !prefixed.isEmpty() )
	    break;
	  thisform = wordform;
	  if ( tag.indexOf(it.first) >= 0 ){
	    // the POS tag matches, so potentially yes
	    UnicodeString part;
	    for ( const auto& part : it.second ){
	      // loop over potential particles.
	      int part_pos = thisform.indexOf(part);
	      if ( part_pos != -1 ){
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
  cout << "created a temprorary mblem trainingsfile: " << filename << endl;
}

void train_mblem( const Configuration& config,
		  const string& datafile,
		  const string& outfile ){
  string timblopts = config.lookUp( "timblOpts", "mblem" );
  string inputfile = temp_dir + datafile;
  cout << "Timbl: Start training Lemmas from: " << inputfile
       << " with Options: '" << timblopts << "'" << endl;
  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( inputfile );
  timbl.WriteInstanceBase( outfile );
  cout << "Timbl: Done, stored Lemma instancebase : " << outfile << endl;
}

void create_lemmatizer( const Configuration& config,
			const multimap<UnicodeString,map<UnicodeString,map<UnicodeString,size_t>>>& data,
			const map<UnicodeString,set<UnicodeString>>& particles,
			const string& mblem_tree_file ){
  if ( data.empty() ){
    cout << "skip creating a lemmatizer, no lemma data available." << endl;
    return;
  }
  string mblem_base = TiCC::basename(mblem_tree_file);
  string mblem_data_file = mblem_base + ".data";
  string output_file = output_dir + mblem_base;
  cout << "create a lemmatizer into: " << output_file << endl;
  create_mblem_trainfile( data, particles, mblem_data_file );
  train_mblem( config, mblem_data_file, output_file );
}

void check_data( Tokenizer::TokenizerClass *tokenizer,
		 const multimap<UnicodeString,map<UnicodeString,map<UnicodeString,size_t>>>& data ){
  for ( const auto& word : data ){
    tokenizer->tokenizeLine( word.first );
    vector<Tokenizer::Token> v = tokenizer->popSentence();
    if ( v.size() != 1 ){
      cerr << "the provided tokenizer doesn't handle '" << word.first
	   << "' well (splits it into " << v.size() << " parts.)" << endl;
      cerr << "[";
      for ( const auto& w : v ){
	cerr << w.us << " ";
      }
      cerr << "]" << endl;
    }
    tokenizer->reset();
  }
}

void add_cgn_files( const string& output_dir,
		    Configuration& config ){
  // copy the cgn files to the output_dir
  string frog_path = string(SYSCONF_PATH) + "/frog/nld/";
  try {
    string infile = frog_path + "cgntags.main";
    if ( !TiCC::isFile( infile ) ){
      throw( "opening: " + infile + " failed: " );
    }
    string outfile = output_dir + "cgntags.main";
    ifstream is( infile );
    ofstream os( outfile );
    os << is.rdbuf();
    config.setatt( "constraints_file", "cgntags.main", "tagger" );
  }
  catch ( const exception& e ){
    cerr << "adding 'cgntags.main' failed: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }
  try {
    string infile = frog_path + "cgntags.sub";
    if ( !TiCC::isFile( infile ) ){
      throw( "opening: " + infile + " failed: " );
    }
    string outfile = output_dir + "cgntags.sub";
    ifstream is( infile );
    ofstream os( outfile );
    os << is.rdbuf();
    config.setatt( "subsets_file", "cgntags.sub", "tagger" );
  }
  catch ( const exception& e ){
    cerr << "adding 'cgntags.sub' failed: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }
  try {
    string infile = frog_path + "cgn_token.trans";
    if ( !TiCC::isFile( infile ) ){
      throw( "opening: " + infile + " failed: " );
    }

    string outfile = output_dir + "cgn_token.trans";
    ifstream is( infile );
    ofstream os( outfile );
    os << is.rdbuf();
    config.setatt( "token_trans_file", "cgn_token.trans", "tagger" );
  }
  catch ( const exception& e ){
    cerr << "adding 'cgn_token.trans' failed: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }

}

int main( int argc, char * const argv[] ) {
  TiCC::CL_Options opts( "b:t:T:l:e:O:c:hV",
			 "help,version,postags:,eos:,lemma-out:,temp-dir:,CGN");
  try {
    opts.parse_args( argc, argv );
  }
  catch ( const exception& e ){
    cerr << e.what() << endl;
    cerr << "use " << opts.prog_name() << " -h for help" << endl;
    exit(EXIT_FAILURE);
  }
  set_default_config();
  string base_name;
  string corpusname;
  string lemma_name;
  string lemma_outname;
  string tokfile;
  string configfile;
  bool use_cgn =  false;
  if ( opts.extract( 'h' ) || opts.extract( "help") ){
    usage( opts.prog_name() );
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'V' ) || opts.extract( "version" ) ){
    cerr << "VERSION: " << VERSION << endl;
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'b', base_name ) ){
    cerr << "option '-b' not longer supported!\n"
	 << "froggen will determine outputfile names based on the names "
	 << "of the input files." << endl;
    return EXIT_FAILURE;
  }
  if ( !opts.extract( 'T', corpusname ) ){
    cout << "Missing a corpus!, (-T option), assuming lemmas only" << endl;
    lemma_file_only = true;
  }
  else if ( !isFile( corpusname ) ){
    cerr << "unable to find the corpus: " << corpusname << endl;
    exit( EXIT_FAILURE );
  }
  else {
    base_name = TiCC::basename( corpusname );
  }
  if ( opts.extract( 'c', configfile ) ){
    if ( !use_config.fill( configfile ) ) {
      cerr << "unable to open:" << configfile << endl;
      exit( EXIT_FAILURE );
    }
    cout << "using configuration: " << configfile << endl;
  }
  use_config.merge( default_config ); // to be sure to have all we need
  opts.extract( 'l', lemma_name );
  if ( !lemma_name.empty() ){
    if ( !isFile(lemma_name) ){
      cerr << "unable to find: '" << lemma_name << "'" << endl;
      return EXIT_FAILURE;
    }
  }
  else if ( lemma_file_only ){
    cerr << "no -T or -l option found!" << endl;
    cerr << "usage: " << opts.prog_name()
	 << " -T taggedcorpus [-l lemmalist] [-c configfile] [-e encoding]"
	 << " [-O outputdir]"
	 << endl;
    cerr << "use " << opts.prog_name() << " -h for more info" << endl;
    exit( EXIT_FAILURE );
  }
  opts.extract( 'O', output_dir );
  if ( !output_dir.empty() ){
    if ( output_dir[output_dir.length()-1] != '/' ){
      output_dir += "/";
    }
    if ( !isWritableDir( output_dir )
	 && !createPath( output_dir ) ){
      cerr << "output dir not usable: " << output_dir << endl;
      exit(EXIT_FAILURE);
    }
  }
  opts.extract( "lemma-out", lemma_outname );
  if ( !lemma_outname.empty()
       && (lemma_outname == lemma_name) ){
    cerr << "conflicting name for lemma-out option " << lemma_outname << endl;
    return EXIT_FAILURE;
  }
  opts.extract( "temp-dir", temp_dir );
  cerr << "TEMP_DIR =" << temp_dir << endl;
  if ( !temp_dir.empty() ){
    if ( temp_dir.back() != '/' ){
      temp_dir += "/";
    }
    if ( !isWritableDir( temp_dir )
	 && !createPath( temp_dir ) ) {
      cerr << "temporary dir '" << temp_dir << "' not usable" << endl;
      return EXIT_FAILURE;
    }
  }
  UnicodeString eos_mark = "<utt>";
  string value;
  opts.extract( "eos", value );
  if ( !value.empty() ){
    eos_mark = TiCC::UnicodeFromUTF8(value);
  }
  bool t_opt = opts.extract( 't', tokfile );
  if ( !t_opt ){
    string tokdir = use_config.getatt( "configDir", "tokenizer" );
    if ( !tokdir.empty() && (tokdir.back() != '/' ) ){
      use_config.clearatt( "configDir", "tokenizer" );
      tokdir += "/";
    }
    string file = use_config.getatt( "rulesFile", "tokenizer" );
    if ( !file.empty() ){
      tokfile = tokdir + file;
    }
  }
  Tokenizer::TokenizerClass *tokenizer = 0;
  if ( !tokfile.empty() ) {
    if ( !isFile(tokfile) ){
      cerr << "unable to find: '" << tokfile << "'" << endl;
      exit( EXIT_FAILURE );
    }
    if ( !output_dir.empty() ){
      // copy the tokenizer file to the output_dir
      string outname = output_dir + TiCC::basename(tokfile);
      ofstream os( outname );
      ifstream is( tokfile );
      os << is.rdbuf();
      tokfile = outname;
    }
    use_config.setatt( "rulesFile", TiCC::basename(tokfile), "tokenizer" );
    if ( t_opt ){
      tokenizer = new Tokenizer::TokenizerClass();
      tokenizer->init( tokfile );
    }
  }
  opts.extract( 'e', encoding );
  string mblem_particles = use_config.lookUp( "particles", "mblem" );
  map<UnicodeString,set<UnicodeString>> particles;
  if ( !mblem_particles.empty() ){
    particles = fill_particles( mblem_particles );
  }
  string pos_tags_file;
  opts.extract( "postags", pos_tags_file );
  use_cgn = opts.extract( "CGN" );
  if ( use_cgn ){
    // copy the CGN files to the output_dir and add them to the config
    add_cgn_files( output_dir, use_config );
  }
  else {
    // just to be sure.
    use_config.clearatt( "constraints_file", "tagger" );
    use_config.clearatt( "subsets_file", "tagger" );
    use_config.clearatt( "token_trans_file", "tagger" );
  }
  if ( !opts.empty() ){
    cerr << "spurious options found: " << opts << endl;
    return EXIT_FAILURE;
  }
  set<UnicodeString> pos_tags = fill_postags( pos_tags_file );
  multimap<UnicodeString,map<UnicodeString,map<UnicodeString,size_t>>> data;
  // WTF is this?
  // a mutimap of Words to a map of lemmas to a frequency list of POS tags.
  // this structure is probably overly complex. redesign is needed.
  // e.g. on output we have te re-sort it to make it usable.
  if ( !lemma_file_only ){
    cout << "start reading lemmas from the corpus: " << corpusname << endl;
    cout << "EOS marker = '" << eos_mark << "'" << endl;
    ifstream corpus( corpusname);
    fill_lemmas( corpus, data, pos_tags, eos_mark );
    if ( debug ){
      cerr << "current data" << endl;
      for ( const auto& it1 : data ){
	cerr << it1.first;
	for( const auto& it2 : it1.second ){
	  cerr << "\t" << it2.first << endl;
	  for( const auto& it3 : it2.second ){
	    cerr << "\t\t\t" << it3.first << " " << it3.second << endl;
	  }
	}
      }
    }
    if ( data.size() == 0 ){
      cout << "no lemma information found. carry on " << endl;
    }
    else {
      cout << "done, current size=" << data.size() << endl;
    }
  }
  if ( !lemma_name.empty() ){
    cout << "start reading extra lemmas from: " << lemma_name << endl;
    ifstream is( lemma_name);
    fill_lemmas( is, data, pos_tags, eos_mark );
    if ( debug ){
      cerr << "current data" << endl;
      for ( const auto& it1 : data ){
	cerr << it1.first;
	for( const auto& it2 : it1.second ){
	  cerr << "\t" << it2.first << endl;
	  for( const auto& it3 : it2.second ){
	    cerr << "\t\t\t" << it3.first << " " << it3.second << endl;
	  }
	}
      }
    }
    cout << "done, total size=" << data.size() << endl;
  }
  if ( debug ){
    cerr << "current data" << endl;
    for ( const auto& it1 : data ){
      cerr << it1.first;
      for( const auto& it2 : it1.second ){
	cerr << "\t" << it2.first << endl;
	for( const auto& it3 : it2.second ){
	  cerr << "\t\t\t" << it3.first << " " << it3.second << endl;
	}
      }
    }
  }
  if ( !lemma_outname.empty() ){
    ofstream os( lemma_outname );
    write_lemmas( os, data );
    cout << "created a lemma file: '" << lemma_outname << "'" << endl;
  }
  string mblem_tree_name = use_config.lookUp( "treeFile", "mblem" );
  if ( mblem_tree_name.empty() ){
    if ( lemma_name.empty() ){
      mblem_tree_name = base_name + ".tree";
    }
    else {
      mblem_tree_name = lemma_name + ".tree";
    }
  }
  string mblem_set_name = use_config.lookUp( "set", "mblem" );
  if ( mblem_set_name.empty() ){
    throw setting_error( "set", "mblem" );
  }
  string tagger_set_name = use_config.lookUp( "set", "tagger" );
  if ( tagger_set_name.empty() ){
    throw setting_error( "set", "mblem" );
  }
  if ( tokenizer ){
    check_data( tokenizer, data );
  }
  Configuration frog_config = use_config;
  if ( !lemma_file_only ){
    create_tagger( use_config, base_name, corpusname, pos_tags, eos_mark );
    frog_config.setatt( "settings", base_name + ".settings", "tagger" );
    frog_config.clearatt( "p", "tagger" );
    frog_config.clearatt( "P", "tagger" );
    frog_config.clearatt( "timblOpts", "tagger" );
    frog_config.clearatt( "M", "tagger" );
    frog_config.clearatt( "n", "tagger" );
    frog_config.clearatt( "%", "tagger" );
  }
  create_lemmatizer( use_config, data, particles, mblem_tree_name );
  frog_config.clearatt( "baseName", "global" );
  frog_config.clearatt( "particles", "mblem"  );
  if ( data.empty() ){
    frog_config.clearatt( "treeFile", "mblem" );
    frog_config.clearatt( "set", "mblem" );
    frog_config.clearatt( "timblOpts", "mblem" );
  }
  else {
    frog_config.setatt( "treeFile", TiCC::basename(mblem_tree_name), "mblem" );
  }
  string frog_cfg = output_dir + "froggen.cfg.template";
  if ( frog_cfg == configfile ){
    frog_cfg += ".new";
  }
  frog_config.create_configfile( frog_cfg );
  cout << "stored a frog configfile template: " << frog_cfg << endl;
  return EXIT_SUCCESS;
}
