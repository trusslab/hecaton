/* Copyright (C) 2018-2021 University of California, Irvine
 * 
 * Author:
 * Seyed Mohammadjavad Seyed Talebi <mjavad@uci.edu>
 * 
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


//==== HecatonPass2.cpp ---------------------------------------------------===//
// This is the second pass of Hecaton which insert bowknots into the functions
//====---------------------------------------------------------------------===//

#include <sstream>
#include <string>
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <fstream>
#include "database_header.h"
#include "fncs_header.h"
#include "clang/Basic/FileManager.h"
using namespace clang;

namespace {
//data structures I am using to keep track of error handling codes


//#define write_to_file(loc,str) TheRewriter.InsertText(loc,str,true,true)
#define write_to_file(loc,str) //TheRewriter.InsertText(loc,str,true,true)
#define write_to_file2(loc,str) TheRewriter.InsertText(loc,str,true,true)
#define write_to_file3(loc,str) TheRewriter.InsertText(loc,str,true,true)
//#define write_to_file(loc,str) //TheRewriter.InsertText(loc,str,true,true)
#define SCORE_NUMBER_EHC 20
#define SCORE_MAYBE_BLOCKS 8
#define SCORE_FNC_POINTER 30
#define SCORE_MISSING_EHC 10


int functionTag =0;
int number_of_gotos =0;
enum EHC_type {GOTO,RETURN,COND,MAYBE,GOTO_MAYBE,VERIFIED,DENIED};
//Hecaton: a data struct to keep the first statement of EHC in the first round of analysis
class EHC_head{
	public:
		EHC_type type;
		Stmt * headStmt;
};
std::list<EHC_head *> EHC_heads;
std::list<EHC_head *>::iterator itEHC_heads;
std::list<EHC_head *>::iterator it2EHC_heads;
class Pair_Candidate{
	public: 
		Stmt * stmt;
		Stmt * pair;
		int probability;
};
class EHC_new_stmt{
	public: 
		Stmt * stmt;
		Stmt * pair;
		uint64_t pairmask_id;
		int is_outer;
		std::list<Stmt*> duplicates;
		std::list<Stmt*>::iterator itduplicates;
		EHC_type type;

};

class ScoredStmt{
    public:	
      Stmt * stmt;
      int score;
};
class OuterEHC{
	public: 
		Stmt * stmt;
		std::list<Stmt*> pairs;
		std::list<Stmt*>::iterator itpairs;
		uint64_t pairmask_id;
		std::list<Stmt*> duplicates;
		std::list<Stmt*>::iterator itduplicates;
		bool found_with_database;
		std::list<int> database_indexes;
		std::list<Stmt*> possible_pairs;
		//std::list<Stmt*> unique_possible_pairs;
		std::list<ScoredStmt*> unique_possible_pairs;
		bool has_inner_duplicate;
		Stmt * inner_duplicate;

};

class OuterSMS{
	public: 
		Stmt * stmt;
		std::string fnc_name;
		std::list<int> database_indexes;
};

class EHC{
	public: 
		EHC_head * head;
		std::list<Stmt*> ehc_stmts;
		std::list<Stmt*>::iterator it_ehc_stmts;
		std::list<EHC_new_stmt *> ehc_new_stmts;
		std::list<EHC_new_stmt *>::iterator it_ehc_new_stmts;
		int return_val;

};
class ConfidenceScore{
	public:
		int score_number_ehc;
		int score_missing_ehc;
		int score_maybe_blocks;
		int score_function_pointer;
		int init_flag;
		int overall_score(){
			return 100 + score_number_ehc + score_missing_ehc + score_function_pointer + score_maybe_blocks;
		}
};
class Function{
	public:
		Stmt * Body;
		std::list<EHC *> EHCs;
		std::list<EHC *>::iterator itEHCs;
		std::list<OuterEHC *> outerEHCs;
		std::list<OuterEHC *>::iterator itouterEHCs;
		std::list<OuterSMS *> outerSMSs;
		std::list<OuterSMS *>::iterator itouterSMSs;
		std::string typeStr;
		std::string name;
		bool is_closing;
		ConfidenceScore confidence_score;


};
std::list<Function*> listOfFunctions;
std::list<Function*>::iterator itFunctions;


class gotosInsertLoc{
	public:
		Stmt * stmt;
		bool should_erase;
		SourceLocation start;
		SourceLocation end;
		SourceLocation aftersemi;


};
std::list<gotosInsertLoc*> gotos_should_insert;
std::list<gotosInsertLoc*> gotos_shouldnt_insert;
std::list<gotosInsertLoc*>::iterator itgotosLocs;
std::list<gotosInsertLoc*>::iterator itgotosLocs2;

bool first_function = true;
// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class HecatonASTVisitor : public RecursiveASTVisitor<HecatonASTVisitor> {
  CompilerInstance &Instance;
  SourceManager &mSM = Instance.getSourceManager();
  ASTContext &mASTcontext = Instance.getASTContext();
public:
  HecatonASTVisitor(CompilerInstance &Instance, Rewriter &R)
      : Instance(Instance),TheRewriter(R) {}
  bool VisitStmt(Stmt *s) {
    return true;
  }

int var_suffix;
std::list<std::string> function_names;
std::list<std::string>::iterator it_function_names;
//Hecaton: to visit all of the functions in the code
  bool VisitFunctionDecl(FunctionDecl *f) {
    // Only function definitions (with bodies), not declarations.
    if (f->hasBody()) {
      if(!mSM.isInMainFile(f->getBeginLoc())){
              return true;
      }
//Hecaton: find basic information of the function and make a Function data struct      
      Stmt *FuncBody = f->getBody();
      if(FuncBody->getBeginLoc().isMacroID()){
	      return true;
      }


      // Type name as string
      QualType QT = f->getReturnType();
      std::string TypeStr = QT.getAsString();

      // Function name
      DeclarationName DeclName = f->getNameInfo().getName();
      std::string FuncName = DeclName.getAsString();

      Function * myfunction = new Function();
      myfunction->Body = FuncBody;
      myfunction->name = FuncName;
      myfunction->typeStr = TypeStr;

      for (itFunctions = listOfFunctions.begin();
		      itFunctions != listOfFunctions.end();
		      ++itFunctions){
          Function * currFunction = (*itFunctions);
	  if(FuncName == currFunction->name){
		  return true;
	  }
      	       
      }
      int fnc_found = 0;
      for (int i=0; i<DATA_SIZE; i++){
                if( FuncName == white_list[i]){
                        fnc_found = 1;
                        break;
                }
      }
      if(fnc_found == 0){
           return true;
      }

#define CL_IND_SIZE  23
      myfunction->is_closing = false;
      std::string closing_indicator[CL_IND_SIZE] = {"remove",
	      "release","exit","destroy","disable","unregister","_put","close",
	      "unmap","delete","del_","cleanup","done","shutdown",
      "deinit","flush","dequeue","free","clean_up","showt_down","unprepare","power_off",
      "clean"};
      for ( int i =0; i<CL_IND_SIZE;  i++){
          if( myfunction->name.find(closing_indicator[i]) != std::string::npos){
	       myfunction->is_closing = true;
	       break;
	  }
      }

      // Hecaton global variable declaration, before first function declaration
      SourceLocation ST = f->getSourceRange().getBegin();
      if(first_function == true){
	  if( ST.isMacroID() ) {
	      ST = mSM.getExpansionRange(ST).getBegin();	
	  }	
          TheRewriter.InsertText(ST, "/*Hecaton global decl*/\n"
			  "#include <linux/sched.h>\n"
			  "#include <linux/hardirq.h>\n"
			  "#include <linux/reboot.h>\n"
			  "#include <linux/delay.h>\n"
			  "extern int hecaton_global_check;\n", true, true);
          TheRewriter.InsertText(ST, "#define hgoto(x) if(unlikely(current->hf)){ if(likely(!in_interrupt())){ goto x;}}\n", true , true);

          TheRewriter.InsertText(ST, "#define hecaton_set_bit(a,b)"
			  "\ta|=b \n", true, true);
          TheRewriter.InsertText(ST, "#define hecaton_unset_bit(a,b)"
			 "\ta&=~b\n", true, true);
          TheRewriter.InsertText(ST, "#define hecaton_check_bit(a,b)"
			 "\ta&b\n", true, true);
	  
	  first_function = false;
      }

      // Hecaotn: to insert function start and end tags
      ST = f->getSourceRange().getBegin();
      write_to_file2(ST, "/*HecatonFunctionStart"+std::to_string(functionTag)+"*/");

      ST = FuncBody->getEndLoc().getLocWithOffset(1);
      write_to_file2(ST,"/*HecatonFunctionEnd"+ std::to_string(functionTag) +"*/");

//Hecaton: to find all of the if condintions inside the every functions bodies
//and find whether the if or else body is EHC
//recursiveVisit_if stores the begining node of each EHC in EHC_heads list
   if(!(myfunction->is_closing)){
      EHC_heads.clear();


      recursiveVisit_function_pointer(FuncBody,myfunction);

      recursiveVisit_if(FuncBody);
//Hecaton: to find every statements in EHCs based on the EHC_heads list
//we need to resolve the goto statements
      find_ehc_stmts(myfunction);

      delete_blacklist_functions(myfunction);
      //to verify MAYBE EHCs based on the database
      handle_maybe(myfunction);

      handle_loops(myfunction);
      handle_critical_sections(myfunction);


      handle_return(myfunction);

////Print found and cleaned EHCs for debugging purposes
      //print_ehcs(myfunction);

//Hecaton: in each EHC we need to find the ehc statements which appears for the first time   
     find_ehc_new_stmts(myfunction);

//Hecaton: find that which statement in the function is being undone by each statemnet 
//in ehc_new_stmts
      find_pairs(myfunction);
      
      find_outer_ehcs(myfunction);

      //for confidence metric
      //find outer state modifying statements
      find_outer_smss(myfunction);

      //check whether there is an error handling statement for each state modifying statement
      score_missing_statements(myfunction);

      number_of_gotos = 0;
      insert_gotos_blocks(FuncBody);
//Hecaton: assing pair mask flags for each EHC
      insert_pairmask_decl(myfunction);
      assign_pairmask_id(myfunction); 
      
      insert_gotos(FuncBody);
      insert_pairmask_stamps(myfunction);
  }else{
      insert_gotos_blocks(FuncBody);
      insert_gotos(FuncBody); 
  }

      std::string clean_up_string = "";
      clean_up_string = generate_cleanup_str(myfunction);
      insert_clean_up_stirng(myfunction , clean_up_string);



	//insert score
      std::string conf_score_str = "/* Confidence Score of this function\n";
      std::stringstream mystream(conf_score_str);
      mystream<<conf_score_str<<"score_number_ehc: "<<myfunction->confidence_score.score_number_ehc<<"\n";
      mystream<<"score_missing_ehc: "<<myfunction->confidence_score.score_missing_ehc<<"\n";
      mystream<<"score_function_pointer: "<<myfunction->confidence_score.score_function_pointer<<"\n";
      mystream<<"score_maybe_blocks: "<<myfunction->confidence_score.score_maybe_blocks<<"\n";
      mystream<<"overall_score: "<<myfunction->confidence_score.overall_score()<<"\n*/\n";
      conf_score_str = mystream.str();
      ST = FuncBody->getEndLoc().getLocWithOffset(1);
      write_to_file2(ST,conf_score_str);

     std::fstream myfile;
     myfile.open ("XXXSCOREPATHXXX", std::fstream::out | std::fstream::app );
     myfile<<myfunction->name<<"\n";
     myfile<<conf_score_str<<"\n\n\n";
     myfile.close();


      listOfFunctions.push_back(myfunction);
      functionTag++;
      


    }

    return true;
  }
void insert_gotos_blocks (Stmt *stmt){
  for (Stmt::child_iterator i = stmt->child_begin(),
			    e = stmt->child_end();
			    i != e;
			    ++i) {
      Stmt *currStmt = (*i);
      if(currStmt==NULL)
	  continue;
      if(isa<CompoundStmt>(currStmt)){
          CompoundStmt * compStmt = cast<CompoundStmt>(currStmt);
	  SourceLocation loc = compStmt->getLBracLoc().getLocWithOffset(1);
	  std::ostringstream o;
	  o<<" hgoto(hecaton_label"<<functionTag<<");";
	  std::string insert_str = o.str();
	  number_of_gotos++;	  
	  write_to_file3(loc , insert_str);
      }
           
      insert_gotos_blocks(currStmt);
  }
}

 void insert_gotos (Stmt *FuncBody){
	//it insert goto hecaton_label#functionTag to the end of all the statements
	//in the function
	  //Stmt * stmt = FuncBody;
	  //uniq_goto_locs.clear();
	  gotos_should_insert.clear();
	  gotos_shouldnt_insert.clear();
  	  gotosVisit(FuncBody,FuncBody);	
	  itgotosLocs = gotos_should_insert.begin();
	  while(itgotosLocs != gotos_should_insert.end()){
              		
	      gotosInsertLoc * gtil = (*itgotosLocs);
              int endline = mSM.getExpansionLineNumber(gtil->end);
              int startline = mSM.getExpansionLineNumber(gtil->start);
	      itgotosLocs2 = itgotosLocs;
	      ++itgotosLocs2;
	      while(itgotosLocs2 != gotos_should_insert.end()){ 
	           gotosInsertLoc * gtil2 = (*itgotosLocs2);
        	   int endline2 = mSM.getExpansionLineNumber(gtil2->end);
        	   int startline2 = mSM.getExpansionLineNumber(gtil2->start);
		   if((endline==endline2)||(startline==startline2)){	   
		       gtil2->should_erase = true;
		       Stmt * ereased_stmt = gtil2->stmt;

		   }	   
		   ++itgotosLocs2;	   
	      }
	      ++itgotosLocs;
	  }	  

	  itgotosLocs = gotos_shouldnt_insert.begin();
	  while(itgotosLocs != gotos_shouldnt_insert.end()){
              		
	      gotosInsertLoc * gtil = (*itgotosLocs);
              int endline = mSM.getExpansionLineNumber(gtil->end);
              int startline = mSM.getExpansionLineNumber(gtil->start);
	      itgotosLocs2 = gotos_should_insert.begin();
	      while(itgotosLocs2 != gotos_should_insert.end()){ 
	           gotosInsertLoc * gtil2 = (*itgotosLocs2);
        	   int endline2 = mSM.getExpansionLineNumber(gtil2->end);
        	   int startline2 = mSM.getExpansionLineNumber(gtil2->start);
		   if((endline==endline2)||(startline==startline2)){	   
		       gtil2->should_erase = true;
		       Stmt * ereased_stmt = gtil2->stmt;

		   }	   
		   ++itgotosLocs2;	    
	      }
	      ++itgotosLocs;
	  }

	  itgotosLocs = gotos_should_insert.begin();
	  while(itgotosLocs != gotos_should_insert.end()){ 
	      gotosInsertLoc * gtil = (*itgotosLocs);
	      if(gtil==NULL){
		      continue;
	      }
              if(gtil->should_erase){
		   itgotosLocs = gotos_should_insert.erase(itgotosLocs); 
	      }else{
	         ++itgotosLocs;
	      }
	  
	  }
	  std::ostringstream o;
	  o<<" hgoto(hecaton_label"<<functionTag<<");\n";
	  std::string insert_str = o.str();
	  for (itgotosLocs = gotos_should_insert.begin();
			  itgotosLocs != gotos_should_insert.end();
			  ++itgotosLocs){
	      gotosInsertLoc * gtil = (*itgotosLocs);
	      number_of_gotos++;
	      write_to_file3(gtil->aftersemi,insert_str);	
	         
	  }

 }
 void gotosVisit (Stmt *stmt, Stmt *Body) { 
	  int should_insert = 1;
  	  for (Stmt::child_iterator i = stmt->child_begin(),
			            e = stmt->child_end();
				    i != e;
				    ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt==NULL)
	          continue;
	      SourceLocation endLoc = currStmt->getEndLoc(); 
	      SourceLocation startLoc = currStmt->getBeginLoc(); 

	      SourceLocation aftersemiLoc = recursiveGetEnd(currStmt);

	      if(!mSM.isInMainFile(aftersemiLoc)){
	          continue;
	      }
	
	      gotosInsertLoc * gtil = new gotosInsertLoc();
	      gtil->end = endLoc;
	      gtil->aftersemi = aftersemiLoc;
	      gtil->start = startLoc;
	      gtil->stmt = currStmt;
	      gtil->should_erase = false;
	      should_insert = 1;
	      Stmt * parent = find_parent_recursive(currStmt,Body);
	      if ( !startLoc.isMacroID()){ 
	         if(isa<ReturnStmt>(currStmt)||isa<ContinueStmt>(currStmt)||
	                     isa<BreakStmt>(currStmt)|| isa<DeclStmt>(currStmt)||
	                     isa<GotoStmt>(currStmt)|| isa<ForStmt>(currStmt)||
	           	  stmt_in_parent_condintion(currStmt,parent)){
	              gotos_shouldnt_insert.push_back(gtil);
	              should_insert = 0;
	         }
	      }
	      if(mSM.isInMainFile(aftersemiLoc) && TheRewriter.isRewritable(aftersemiLoc)){
		  char ch;
	          std::string mystr = GetStmtString2(gtil->stmt);
	          ch = mystr[mystr.length()-1];
	          if((should_insert ==1)&&(ch==';')){     
		       gotos_should_insert.push_back(gtil);
		  } 
	      }

	      gotosVisit(currStmt,Body);	   
	  }
 }
int check_for_sms_keyword(std::string fnc_name){

	int size = 18;
	std::string keyword[18] = { "get", "create", "init", "register", "lock", "up", "enable" , "add" , "inc" , "alloc" , "start", "resume", "connect", "map", "enqueue", "prepare", "attach", "beign"};
	for (int i =0; i<size; i++){
		auto pos = fnc_name.find(keyword[i]);
		if (pos != std::string::npos){
			return 1;
		}
	}
	return 0;
}
 void recursiveVisit_function_pointer (Stmt *stmt, Function * myfunction) {
  	  for (Stmt::child_iterator i = stmt->child_begin(),
			            e = stmt->child_end();
				    i != e;
				    ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if( isa<CallExpr>(currStmt) ){
		       std::string func_str;
		       func_str = GetStmtString(currStmt);
		       auto pos = func_str.find("(");
		       if( pos != std::string::npos){
		            auto fnc_name = func_str.substr(0,pos);   
		       	    auto pos2 = fnc_name.find("->");
	      		     int sms_keyword_found = 0;
		       	    if( pos2 != std::string::npos){
				sms_keyword_found = check_for_sms_keyword(fnc_name);
				if(sms_keyword_found){
					if( myfunction->confidence_score.init_flag == 1392){
					    myfunction->confidence_score.score_function_pointer -=  SCORE_FNC_POINTER;
					}else{
					    myfunction->confidence_score.score_function_pointer = - SCORE_FNC_POINTER;
					    myfunction->confidence_score.init_flag = 1392;
					}
				}
			    }

		       }
		    }
                    recursiveVisit_function_pointer(currStmt, myfunction);
	      }
	   } 
	   return;
}
 void recursiveVisit_if (Stmt *stmt) {
  	  for (Stmt::child_iterator i = stmt->child_begin(),
			            e = stmt->child_end();
				    i != e;
				    ++i) {
	      int maybe = 0;	  
	      int cond_found =0;
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if(isa<IfStmt>(currStmt)){
		         IfStmt *IfStatement = cast<IfStmt>(currStmt);
		         Stmt *Then = IfStatement->getThen();
		         write_to_file(Then->getBeginLoc(),"// the 'if' part");
		         write_to_file(Then->getEndLoc(),"// the 'if' part ends\n");

		         Expr *condition = IfStatement->getCond();
			 if(condition == NULL)
			 	continue;
		 	maybe = 1; 
			//Handle cases like if(rc<0) 	
		         if(isa<BinaryOperator>(condition)){
		           	BinaryOperator *binop = cast<BinaryOperator>(condition);
		           	if(binop->isComparisonOp ()){	
		           		write_to_file(Then->getBeginLoc(),"// has a comparison oprator");
		           		write_to_file(Then->getBeginLoc(),binop-> getOpcodeStr().str());
		           		Expr *right_hand = binop->getRHS();
					if(right_hand!=NULL)
		           		if(isa<IntegerLiteral>(right_hand)){
		           			IntegerLiteral* int_right_hand = cast<IntegerLiteral>(right_hand);
		           			int value = int_right_hand->getValue().getLimitedValue();
		           			write_to_file(Then->getBeginLoc(),std::to_string(value));
		           			if ((binop->getOpcodeStr().str() == "<") && (value==0)){
		           				write_to_file(Then->getBeginLoc(),"\t this is an error handling code");
							EHC_head * ehc_head = new EHC_head();
							ehc_head->type = COND;
							ehc_head->headStmt = Then;
							EHC_heads.push_back(ehc_head);
							cond_found = 1;
		           			}

		           		}
		           	}
		         }
			//Handle cases like if(IS_ERR_OR_NULL(ptr))
      			 if( isa<CallExpr>(condition) ){
      			         CallExpr * callexp = cast<CallExpr>(condition);
      			         FunctionDecl * func_decl = callexp->getDirectCallee();
      			         if(func_decl !=0){
      			   	      std::string func_name = func_decl->getNameInfo().getAsString();
      			   	      if( (func_name == "IS_ERR") || func_name == "IS_ERR_OR_NULL"){
						write_to_file(Then->getBeginLoc(),"\t this is an error handling code");
							EHC_head * ehc_head = new EHC_head();
							ehc_head->type = COND;
							ehc_head->headStmt = Then;
							EHC_heads.push_back(ehc_head);
							cond_found = 1;
      			   	      }

      			         }
      			 }
			
			 int return_neg_value;
			 return_neg_value = Visit_return(Then);
			 //Handle cases like if(..){.. return -ENUMBER}
			 if(cond_found == 0){
			 // Becuase we might have cases like if(rc<0){.. return -ENUM} // we do not want to count these blocks twice	 
			     if ( return_neg_value == 1){
			            write_to_file(Then->getBeginLoc(),"  This is an EHC becuase of negative return value\n");
			            EHC_head * ehc_head = new EHC_head();
			            ehc_head->type = RETURN;
			            ehc_head->headStmt = Then;
			            EHC_heads.push_back(ehc_head);
			     } else if ( return_neg_value == 2){
			            write_to_file(Then->getBeginLoc(),"  This MAYBE EHC [1]\n");
			            EHC_head * ehc_head = new EHC_head();
			            ehc_head->type = MAYBE;
			            ehc_head->headStmt = Then;
			            EHC_heads.push_back(ehc_head);
			     }
			 }
			 //Handle cases like if(..){.. return ...}
			 
			 //Handle cases like if(..){.. goto}
			 int has_goto;
			 char gotostr[512];
      			 has_goto = Visit_goto(Then,gotostr);
			 std::string gotostring(gotostr);
			 if ( has_goto == 1){
				if(cond_found == 1){
				    EHC_heads.back()->type = GOTO;
				    write_to_file(Then->getBeginLoc(),"  This is an EHC becuase of a goto\n");
				}else if(cond_found ==0){
				    EHC_head * ehc_head = new EHC_head();
				    std::size_t err_found = gotostring.find("err");
				    if( err_found != std::string::npos){
				    	ehc_head->type = GOTO;
				    	write_to_file(Then->getBeginLoc(),"  This is EHC becuase of a goto and err in the lable\n");
				    }else{
				    	ehc_head->type = GOTO_MAYBE;
				    	write_to_file(Then->getBeginLoc(),"  This MAYBE EHC becuase of a goto\n");
				    }
				    ehc_head->headStmt = Then;
				    EHC_heads.push_back(ehc_head);
				}
			 }

		         Stmt *Else = IfStatement->getElse();
		         if (Else){
		           write_to_file(Else->getBeginLoc(),"// the 'else' part");
		           write_to_file(Else->getEndLoc(),"// the 'else' part end\n");
			   int return_neg_value;
			   return_neg_value = Visit_return(Else);
			   //Handle if(..){...}else{..return -ENUMBER}
			   if ( return_neg_value == 1){
			          write_to_file(Else->getBeginLoc(),"  This is an EHC becuase of negative return value\n");
			          EHC_head * ehc_head = new EHC_head();
			          ehc_head->type = RETURN;
			          ehc_head->headStmt = Else;
			          EHC_heads.push_back(ehc_head);
			   } else if (return_neg_value == 2){
			          write_to_file(Else->getBeginLoc()," This MAYBE EHC [2] \n");
			          EHC_head * ehc_head = new EHC_head();
			          ehc_head->type = MAYBE;
			          ehc_head->headStmt = Else;
			          EHC_heads.push_back(ehc_head);

			   }
			   int has_goto;
			   //Handle if(..){...}else{..goto}
      			   has_goto = Visit_goto(Else,gotostr);
			   if ( has_goto == 1){
			          EHC_head * ehc_head = new EHC_head();
				  std::size_t err_found = gotostring.find("err");
				  if( err_found != std::string::npos){
				  	ehc_head->type = GOTO;
				  	write_to_file(Else->getBeginLoc(),"  This is EHC becuase of a goto and err in the lable\n");
				  }else{
				  	ehc_head->type = GOTO_MAYBE;
				  	write_to_file(Else->getBeginLoc(),"  This MAYBE EHC becuase of a goto\n");
				  }
			          ehc_head->headStmt = Else;
			          EHC_heads.push_back(ehc_head);
			   }
		         }

		    }
                    recursiveVisit_if(currStmt);
	      }
	   } 
	   return;
}
int  Visit_return (Stmt *stmt) {
	  int ret = 0 ;
  	  for (Stmt::child_iterator i = stmt->child_begin(), e = stmt->child_end(); i != e; ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if(isa<ReturnStmt>(currStmt)){
			// we use 2 for return in general; 1 for return -ENUM
			ret =2;
	   		ReturnStmt * RetStatement = cast<ReturnStmt>(currStmt);
	   		SourceLocation srcloc = RetStatement->getReturnLoc();
      	   		write_to_file(srcloc,"//return statement ");
	   		Expr *retval = RetStatement->getRetValue();
	   		if(retval!=NULL){
	   		        if(isa<IntegerLiteral>(retval)){
	   		     	IntegerLiteral* int_retval = cast<IntegerLiteral>(retval);
	   		     	int value = int_retval->getValue().getLimitedValue();
      	   				write_to_file(srcloc,std::to_string(value));
	   		        }
				if(isa<ImplicitCastExpr>(retval)){
					ImplicitCastExpr * ImCaEx = cast<ImplicitCastExpr>(retval);
					retval = ImCaEx -> getSubExprAsWritten();
				}
	   		        if(isa<UnaryOperator>(retval)){
	   		     	   std::string uniop_str="";   
	   		     	   UnaryOperator* uniop = cast<UnaryOperator>(retval);
	   		     	   if (uniop->getOpcode() == UO_Minus){

	   		     		uniop_str = " - ";
					// we use 2 for return in general; 1 for return -ENUM
					ret = 1;
      	   				write_to_file(srcloc," has UnaryOperator" + uniop_str);
      	   				write_to_file(srcloc,"\n");
					break;
	   		     	   }
      	   			   write_to_file(srcloc," has UnaryOperator" + uniop_str);
	   		        }
	   		}
      	   		write_to_file(srcloc,"\n");

		    }
		    if(isa<BreakStmt>(currStmt)){
			//we return 2 for break statements show the possibility of EHB    
			ret = 2;    
	   		BreakStmt * BreakStatement = cast<BreakStmt>(currStmt);
	   		SourceLocation srcloc = BreakStatement->getBreakLoc();
      	   		write_to_file(srcloc,"//break statement\n ");
			break;
		    }
		    if(isa<ContinueStmt>(currStmt)){
			//we return 2 for break statements show the possibility of EHB    
			ret = 2;    
	   		ContinueStmt * ContinueStatement = cast<ContinueStmt>(currStmt);
	   		SourceLocation srcloc = ContinueStatement->getContinueLoc();
      	   		write_to_file(srcloc,"//continue statement\n ");
			break;
		    }
	      }
	   } 
	   return ret;
}
 int Visit_goto (Stmt *stmt, char * out_string) {
	  int ret = 0;
 	  std::string tmp_string;
	  
	  if(stmt!=NULL){
	       if(isa<GotoStmt>(stmt)){
		       return 1;
	       }
	  }
  	  for (Stmt::child_iterator i = stmt->child_begin(), e = stmt->child_end(); i != e; ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if(isa<GotoStmt>(currStmt)){
			   tmp_string =  GetStmtString(currStmt);
			   std::strcpy(out_string, tmp_string.c_str());
			   ret=1;
			   break;
		    }
	      }
	   } 
	   return ret;
}

void find_ehc_stmts(Function * myfunction){

      for (itEHC_heads = EHC_heads.begin() ;
		      itEHC_heads != EHC_heads.end();
		      ++itEHC_heads){
	      EHC_head * my_ehc_head = (*itEHC_heads);
      	      EHC * myehc = new EHC();
	      myehc->head = my_ehc_head;
	      myehc->ehc_stmts.clear();
//Hecaton:resolve the goto
	      if(my_ehc_head->type == GOTO || my_ehc_head->type == GOTO_MAYBE){
		      GotoStmt * goto_stmt;
		      LabelDecl * labeldec;
		      LabelStmt * labelstm;
	     	      if ( my_ehc_head->headStmt == NULL){
			      continue;
		      }
		      for (Stmt::child_iterator i = my_ehc_head->headStmt->child_begin(),
				      e = my_ehc_head->headStmt->child_end();
				      i != e;
				      ++i) {
			      Stmt *currStmt = (*i);
			      if(currStmt!=NULL){
				    if(isa<GotoStmt>(currStmt)){
			   		goto_stmt = cast<GotoStmt>(currStmt);
					labeldec = goto_stmt->getLabel();
					labelstm = labeldec->getStmt();

				    }else{
			      		myehc->ehc_stmts.push_back(currStmt); 
				    }
			      }
		      }
		      int start=0;
		      Stmt * child_stmt = cast<Stmt>(labelstm);
		      Stmt* lable_parent = find_parent_recursive(child_stmt,myfunction->Body);
		      //lable_parent = myfunction->Body;
		      for (Stmt::child_iterator i = lable_parent->child_begin(),
				      e = lable_parent->child_end();
				      i != e;
				      ++i) {
			      Stmt *currStmt = (*i);
			      if(currStmt == labelstm){
				      start = 1;
			      }
			      if(currStmt==NULL)
				      continue;
			      if(start==1){
				      if(isa<LabelStmt>(currStmt)){    
				          Stmt * newStmt = (*(currStmt->child_begin()));
					  if(newStmt !=NULL){
					     while( isa<LabelStmt>(newStmt) ){
				                   newStmt = (*(newStmt->child_begin()));
					     }
					     
					     myehc->ehc_stmts.push_back(newStmt);
					  }
				      }else{
					  myehc->ehc_stmts.push_back(currStmt);
				      }
			      }
			      if(isa<ReturnStmt>(currStmt)){
				      start = 0;
			      }

		      }
//Hecaton: resovle the goto end

	      }else{
//Hecaton: if EHC does not have a goto simply add all the child statements to EHC statment list		      
//				
                        if(my_ehc_head !=NULL){ 
			 if(my_ehc_head->headStmt!=NULL){
		              if(isa<CompoundStmt>(my_ehc_head->headStmt)){
		                  for (Stmt::child_iterator i = my_ehc_head->headStmt->child_begin(),
		                    	      e = my_ehc_head->headStmt->child_end();
		                    	      i != e;
		                    	      ++i) {
		                          Stmt *currStmt = (*i);
                                          if(currStmt==NULL){
                                            continue;
					  } 
		                          myehc->ehc_stmts.push_back(currStmt);
		                  }
		              }else{

		                          myehc->ehc_stmts.push_back(my_ehc_head->headStmt);
		              }
                         }
			}
	      }
	      myfunction->EHCs.push_back(myehc);
      }
}

void handle_maybe(Function * myfunction){
      int total_fncs = 0;
      int verified_fncs =0; 
      int counter =0;    
      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){
              EHC * ehc = (*myfunction->itEHCs);
	      SourceLocation loc = ehc->head->headStmt->getEndLoc();

	      if (( ehc->head->type != MAYBE ) && (ehc->head->type != GOTO_MAYBE)){
	           continue;
	      }
              for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();ehc->it_ehc_stmts!=ehc->ehc_stmts.end();++ehc->it_ehc_stmts){
        	     Stmt *currStmt = (*ehc->it_ehc_stmts);
                     if (isa<CallExpr>(currStmt)){
                         CallExpr * callexpr = cast<CallExpr>(currStmt);
                         FunctionDecl * fncdcl = callexpr->getDirectCallee();
	                 if(fncdcl != NULL){
                             DeclarationName declname = fncdcl->getNameInfo().getName();
                             std::string fncname = declname.getAsString();

			     total_fncs ++;
			     std::list<int> founds =
				   find_fnc(ehc_fncs, DATABASE_SIZE, fncname);  
			     if(!(founds.empty())){
			         verified_fncs++;
			     }else{
			     }

			 }
		     }
              }
	      if(( verified_fncs >= 1 )||(total_fncs == 0)){
		  ehc->head->type = VERIFIED;
                  
	      }else{
		  ehc->head->type = DENIED;
                  
	      }
              for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();ehc->it_ehc_stmts!=ehc->ehc_stmts.end();++ehc->it_ehc_stmts){
        	     Stmt *currStmt = (*ehc->it_ehc_stmts);
		     if (isa<LabelStmt>(currStmt)){
		         ehc->head->type = DENIED;
			 break;
		     }
	      }


      }

}
void handle_loops(Function * myfunction){
      std::list<SourceLocation*>::iterator it_srcloc;
      std::list<SourceLocation*> srclocs;
      srclocs.clear();
      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){
              EHC * ehc = (*myfunction->itEHCs);
              for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();ehc->it_ehc_stmts!=ehc->ehc_stmts.end();){
        	      bool delete_this_stmt = false;
        	      Stmt *currStmt = (*ehc->it_ehc_stmts);
//Hecaton: handle loops	
		      //FIXME
		      if(currStmt==NULL){
        		  ++(ehc->it_ehc_stmts); 
			  continue;	 
		      }	  
		      std::string loop_list[] = {
			      "for(",
			      "for (",
			      "while(",
			      "while ("
		      };
		      int loop_list_size = 4;
		      std::string stmt_string2;

	              SourceRange src_range = currStmt->getSourceRange();
		      if( src_range.getBegin().isMacroID() || src_range.getEnd().isMacroID() ) {
			   src_range = TheRewriter.getSourceMgr().getExpansionRange(src_range).getAsRange();	
		      }
		      stmt_string2 = TheRewriter.getRewrittenText(src_range);
		      if(!stmt_string2.empty() ){
		         for (int i=0 ; i<loop_list_size ; i++){
		             if( (stmt_string2.find(loop_list[i]) != std::string::npos) ){
		          	delete_this_stmt = true;
		             }   
		         }
		      }
        	      if(delete_this_stmt){
        		      ehc->it_ehc_stmts = ehc->ehc_stmts.erase(ehc->it_ehc_stmts); 
			      SourceLocation loc = src_range.getEnd();
			      int not_uniq = 0;
			      for( it_srcloc = srclocs.begin();
			        	      it_srcloc != srclocs.end();
			        	      ++it_srcloc){
		                  SourceLocation * curloc = (*it_srcloc);
			          if( curloc != NULL){
			             if ( *curloc == loc){
			                  not_uniq = 1;
			             }
			          }
			      }
			      if(not_uniq == 0){
			      }
			      srclocs.push_back(&loc);
        	      }else{
        		      ++(ehc->it_ehc_stmts);
        	      }
        	      
              }

      }

}
void handle_critical_sections(Function * myfunction){

      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){
              EHC * ehc = (*myfunction->itEHCs);
              for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();ehc->it_ehc_stmts!=ehc->ehc_stmts.end();){
        	      bool delete_this_stmt = false;
        	      Stmt *currStmt = (*ehc->it_ehc_stmts);
//Hecaton: handle the locks in EHCs	
		      if(currStmt==NULL){
        		  ++(ehc->it_ehc_stmts); 
			  continue;	 
		      }	  
		      std::string lock_list[] = {
			      "mutex_lock",
			      "mutex_lock_interruptible",
			      "_raw_read_lock",
			      "spin_lock",
			      "spin_lock_bh",
			      "spin_lock_irq",
			      "write_seqcount_begin"
		      };
		      int lock_list_size = 7;
		      std::string stmt_string;
		      llvm::raw_string_ostream stream(stmt_string);

		      currStmt->printPretty(stream, NULL, PrintingPolicy(LangOptions()));
		      stream.flush();

		      for (int i=0 ; i<lock_list_size ; i++){
		          if( (stmt_string.find(lock_list[i]) != std::string::npos) ){
		          	delete_this_stmt = true;
		          }
		      }
		       
        	      if(delete_this_stmt){
        		      ehc->it_ehc_stmts = ehc->ehc_stmts.erase(ehc->it_ehc_stmts); 
			      SourceLocation loc = currStmt->getEndLoc();
        	      }else{
        		      ++(ehc->it_ehc_stmts);
        	      }
        	      
              }

      }

}
void delete_blacklist_functions(Function * myfunction){
      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){
              EHC * ehc = (*myfunction->itEHCs);
              for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();ehc->it_ehc_stmts!=ehc->ehc_stmts.end();){
        	      bool delete_this_stmt = false;
        	      Stmt *currStmt = (*ehc->it_ehc_stmts);
//Hecaton: handle the printk	
		      if(currStmt==NULL){
        		  ++(ehc->it_ehc_stmts); 
			  continue;	 
		      }	  
		      std::string blacklist[] = {
			      "print",
			      "pr_",
			      "break",
			      "continue",
			      "goto",
			      "__builtin_",
			      "rc =",
			      "rc=",
			      "dev_err",
			      "dev_crit",
			      "dev_info",
			      "dev_warn",
			      "cam_debug_log",
			      "cam_get_module_name",
			      "dump_stack",
			      "ERR_CAST",
			      "ERR_PTR",
			      "IS_ERR",
			      "netdev_err",
			      "netdev_info",
			      "netdev_priv",
			      "netdev_warn",
			      "pd_engine_log",
			      "PTR_ERR",
			      "sde_evtlog_log",
			      "strcmp",
			      "strlcpy",
			      "strncasecmp",
			      "strncmp",
			      "panic",
			      "write_seqcount_end"
		      };
		      int black_list_size = 31;
		      std::string stmt_string;
		      llvm::raw_string_ostream stream(stmt_string);

		      currStmt->printPretty(stream, NULL, PrintingPolicy(LangOptions()));
		      stream.flush();

		      for (int i=0 ; i<black_list_size ; i++){
		          if( (stmt_string.find(blacklist[i]) != std::string::npos) ){
		          	delete_this_stmt = true;
		          }
		      }
		       
        	      if(delete_this_stmt){
        		      ehc->it_ehc_stmts = ehc->ehc_stmts.erase(ehc->it_ehc_stmts); 
        	      }else{
        		      ++(ehc->it_ehc_stmts);
        	      }
        	      
              }

      }

}

void handle_return(Function * myfunction){
      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){
              EHC * ehc = (*myfunction->itEHCs);
              for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();ehc->it_ehc_stmts!=ehc->ehc_stmts.end();){
        	      bool delete_this_stmt = false;
        	      Stmt *currStmt = (*ehc->it_ehc_stmts);
//Hecaton: handle return statements
		        if (currStmt==NULL){
        		    ++(ehc->it_ehc_stmts);
	  	            continue;
			}			
		      if(isa<ReturnStmt>(currStmt)){
			  delete_this_stmt = true;
		          std::string retstr = "";
			  int ret_int=0;
	   	          ReturnStmt * RetStatement = cast<ReturnStmt>(currStmt);
	   	          Expr *retval = RetStatement->getRetValue();
	   	          if(retval!=NULL){
//Hecaton: handle the case : return rc				  
			      if(isa<ImplicitCastExpr>(retval)){
			              ImplicitCastExpr * retval2 = cast<ImplicitCastExpr>(retval);
			              Expr * retval3  = retval2->getSubExpr();
				      if(retval3!=NULL)
			              if(isa<DeclRefExpr>(retval3)){
			                   DeclRefExpr * retval4 =cast<DeclRefExpr>(retval3);
			                   DeclarationName declname = retval4->getNameInfo().getName();
			                   retstr = declname.getAsString();
					   std::list<Stmt*>::iterator it_ehc_stmts2;
//Hecaton: now we know that we have return rc; looking for rc = -s.th
              				   for (it_ehc_stmts2 = ehc->ehc_stmts.begin();it_ehc_stmts2 != ehc->ehc_stmts.end(); ){
        	                               bool delete_this_stmt2 = false;
						
        	      			       Stmt *currStmt2 = (*it_ehc_stmts2);
					       if(currStmt2!=NULL)
					       if(isa<BinaryOperator>(currStmt2)){
				                   BinaryOperator * binop = cast<BinaryOperator>(currStmt2);
						   if(binop->getOpcode() == BO_Assign ){
							Expr * lhs = binop->getLHS();
							if(lhs!=NULL)
							if(isa<DeclRefExpr>(lhs)){
							    std::string rcstr;
							    DeclRefExpr * dcl =cast<DeclRefExpr>(lhs);
							    DeclarationName dclname = dcl->getNameInfo().getName();
							    rcstr = dclname.getAsString();
							    if(rcstr == retstr){
							        Expr * rhs = binop->getRHS();
								if(rhs!=NULL)
			                                        if(isa<UnaryOperator>(rhs)){
			                                            UnaryOperator* uniop = cast<UnaryOperator>(rhs);
			                                            if (uniop->getOpcode() == UO_Minus){
			                                                Expr *retvalue =  uniop->getSubExpr();
									if(retvalue!=NULL)
			                                                if(isa<IntegerLiteral>(retvalue)){
			                                               	 IntegerLiteral* int_val = cast<IntegerLiteral>(retvalue);
			                                               	 ret_int = int_val->getValue().getLimitedValue();
									 delete_this_stmt2 =true;
			                                                }

			                                            }
			                                        }


							    }
							}
							
						   }
					       }
					       if(delete_this_stmt2){
					           it_ehc_stmts2 = ehc->ehc_stmts.erase(it_ehc_stmts2);
					       }else{
					           it_ehc_stmts2++;
					       }

					   }
			              }
			      }
//HEcaton: handle the case : retrun -ENUM

			      if(isa<ImplicitCastExpr>(retval)){
			          ImplicitCastExpr * ImCaEx = cast<ImplicitCastExpr>(retval);
			          retval = ImCaEx -> getSubExprAsWritten();
			      }
			      if(isa<UnaryOperator>(retval)){
			          std::string uniop_str="";   
			          UnaryOperator* uniop = cast<UnaryOperator>(retval);
			          if (uniop->getOpcode() == UO_Minus){
			              Expr *retvalue =  uniop->getSubExpr();
				      if(retvalue!=NULL)
			              if(isa<IntegerLiteral>(retvalue)){
			             	 IntegerLiteral* int_val = cast<IntegerLiteral>(retvalue);
			             	 ret_int = int_val->getValue().getLimitedValue();
			              }

			          }
			      }
			      ehc->return_val = ret_int;
	   	          }
		      }
		       
        	      if(delete_this_stmt){
        		      ehc->it_ehc_stmts = ehc->ehc_stmts.erase(ehc->it_ehc_stmts); 
        	      }else{
        		      ++(ehc->it_ehc_stmts);
        	      }
        	      
              }

      }

}


void  print_ehcs(Function *myfunction){

      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){

          EHC * ehc = (*myfunction->itEHCs);
         
          for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();
            	      ehc->it_ehc_stmts!=ehc->ehc_stmts.end();
            	      ++ehc->it_ehc_stmts){
                  Stmt *currStmt = (*ehc->it_ehc_stmts);
		  if(currStmt!=NULL)
                  currStmt->dumpPretty(mASTcontext); 
          }

      }
}
bool is_in_macro( Stmt * stmt , Stmt * Body){
    Stmt * parent = find_parent_recursive(stmt,Body);
    if (parent == NULL){
        return false;
    }
    if((stmt->getBeginLoc().isMacroID())&&
		    (parent->getBeginLoc().isMacroID())){
          return true;
    }
    return false;
}
void  find_ehc_new_stmts(Function * myfunction){

      std::list<EHC *>::iterator itEHCs2;
      std::list<Stmt*>::iterator it_ehc_stmts2;
      bool is_new_statement = false;
      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){

	  EHC * ehc = (*myfunction->itEHCs);
	  if(ehc->head->type == DENIED){
		 continue;
	  }
	  for (ehc->it_ehc_stmts = ehc->ehc_stmts.begin();
	    	      ehc->it_ehc_stmts != ehc->ehc_stmts.end();
	    	      ++ehc->it_ehc_stmts){

	      Stmt *currStmt = (*ehc->it_ehc_stmts);
	      is_new_statement = true;
	      if(currStmt==NULL)
		      continue;
      	      for (itEHCs2 = myfunction->EHCs.begin();
      	        	     itEHCs2 != myfunction->EHCs.end();
      	        	      ++itEHCs2){

	          EHC * ehc2 = (*itEHCs2);
		  if(ehc2 == ehc){
		      break;
		  }
	          for (it_ehc_stmts2 = ehc2->ehc_stmts.begin();
	            	      it_ehc_stmts2 != ehc2->ehc_stmts.end();
	            	      ++it_ehc_stmts2){
	              Stmt *currStmt2 = (*it_ehc_stmts2);
		      if(currStmt2==NULL)
			      continue;

		      std::string stmt_string;
		      std::string stmt_string2;
		      llvm::raw_string_ostream stream(stmt_string);
		      llvm::raw_string_ostream stream2(stmt_string2);

		      currStmt->printPretty(stream, NULL, PrintingPolicy(LangOptions()));
		      currStmt2->printPretty(stream2, NULL, PrintingPolicy(LangOptions()));
		      stream.flush();
		      stream2.flush();

		      if(stmt_string2 == stmt_string){
			      is_new_statement = false;
			      break;
		      }


	          }
      	      }
	      if(is_new_statement){
		      //Filter out in Macro Statements
		      if( is_in_macro(currStmt, myfunction->Body) ){
		      }else{
		         // currStmt->dumpPretty(mASTcontext);
		          EHC_new_stmt * ehc_new_stmt = new EHC_new_stmt();
		          ehc_new_stmt->stmt = currStmt ;
			  ehc_new_stmt->type = ehc->head->type;
		          ehc->ehc_new_stmts.push_back(ehc_new_stmt);
		      }
		      
	      } 
	  }
      }

}
std::string generate_cleanup_str (Function * myfunction){
	std::string outstr = "";
	if(myfunction->is_closing){
	
	    outstr = "\tif(hecaton_global_check < 0){\n\thecaton_label" +
		    std::to_string(functionTag)+":\n"+
		    "\t\tprintk(KERN_ALERT\"Hecaton reboot on EHC itself "+ myfunction->name +"\"); /*HecatonJump" + std::to_string(functionTag) +"*/"  +
		"\n\t\tmsleep(2000);\n\t\tkernel_restart(NULL);\n\t}\n";
	    return outstr;

	}
	int ret_void = 0;
	if( myfunction->typeStr.find("enum") != std::string::npos ){
		outstr = "\treturn -1;\n}\n";
	}else if ( (myfunction->typeStr == "int")||
             (myfunction->typeStr == "long")|| 
	     (myfunction->typeStr == "int32_t")||
	     (myfunction->typeStr == "int64_t")
	     )	{
		outstr = "\treturn -1;\n}\n";
	}else if (
	     (myfunction->typeStr == "uint64_t")||
	     (myfunction->typeStr == "uint32_t")||
	     (myfunction->typeStr == "unsigned int")||
	     (myfunction->typeStr == "ssize_t")||
	     (myfunction->typeStr == "unsigned long")
		 ){	
		outstr = "\treturn 0;\n}\n";
	}else if(myfunction->typeStr == "irqreturn_t"){
		outstr = "\treturn IRQ_HANDLED;\n}\n";
	}else if ( myfunction->typeStr == "void" ){
	    ret_void = 1;	
	    outstr = "\treturn;\n}\n";
	}else{
		outstr = "\treturn NULL;\n}\n";
	}
        outstr = "\tcurrent->hf = 1;\n" + outstr;
	for( myfunction->itEHCs  = myfunction->EHCs.begin();
	     myfunction->itEHCs != myfunction->EHCs.end();
	     ++myfunction->itEHCs){

		EHC * ehc = (*myfunction->itEHCs);
		std::string ehc_str = "";
		int ehc_new_stmt_count = 0;

		for( ehc->it_ehc_new_stmts  = ehc->ehc_new_stmts.begin();
		     ehc->it_ehc_new_stmts != ehc->ehc_new_stmts.end();
     		   ++ehc->it_ehc_new_stmts){
			ehc_new_stmt_count ++;
			EHC_new_stmt * newstmt = (*ehc->it_ehc_new_stmts); 
			if( newstmt->is_outer){
			    continue;
			}
	                Stmt *currStmt = newstmt->stmt;
			if(newstmt->type == VERIFIED){
		           //Confidence Score
		  	      if( myfunction->confidence_score.init_flag == 1371){
		  	          myfunction->confidence_score.score_maybe_blocks -=  SCORE_MAYBE_BLOCKS;
		  	      }else{
		  	          myfunction->confidence_score.score_maybe_blocks = - SCORE_MAYBE_BLOCKS;
		  	          myfunction->confidence_score.init_flag = 1371;
		  	      }
			}
			std::string stmt_str = "";
			//Hecaton: Using rewriter to get the unexpanded
			//string for the statement
			stmt_str = GetStmtString(currStmt);
			std::ostringstream o;
			o<<newstmt->pairmask_id;
			std::string bin_mask = o.str();
			std::string condition = "\tif( hecaton_check_bit(hecaton_pairmask, "
				+ bin_mask + ") )\n\t";
			ehc_str = ehc_str + condition +"\t" + stmt_str + ";\n" ;

		        	

		}	
		if(ehc->return_val !=0 && ehc_new_stmt_count !=0){
		}
		outstr = ehc_str + outstr ;		
	}
	for( myfunction->itouterEHCs  = myfunction->outerEHCs.begin();
	     myfunction->itouterEHCs != myfunction->outerEHCs.end();
	     ++myfunction->itouterEHCs){

		OuterEHC * ehc_outer = (*myfunction->itouterEHCs);
		std::string ehc_outer_str = "//outer block\n";

		Stmt *currStmt = ehc_outer->stmt;

		std::string stmt_str = "";
		stmt_str = GetStmtString(currStmt);
		std::ostringstream o;
		o<<ehc_outer->pairmask_id;
		std::string bin_mask = o.str();
		std::string condition = "\tif( hecaton_check_bit(hecaton_pairmask, "
			+ bin_mask + ") )\n\t";
		ehc_outer_str = ehc_outer_str + condition +"\t" + stmt_str + ";\n" ;

		        	

		outstr = ehc_outer_str + outstr ;		
	}
	std::string retStr = "";
	std::string print_str = ""; 
	std::string label_str = "";
	std::string hf_str = "current->hf = 0;";
	if(number_of_gotos>0){
	    label_str = "hecaton_label" + std::to_string(functionTag)+":" ;
	   // if( ret_void == 0)
	   // 	hf_str = "current->hf=0;";
	//javad}
        
	print_str = "printk(KERN_ALERT\"Hecaton's bowknot cleanup "+ myfunction->name +", pair_mask =%llx, hecaton_test=%llx\\n\", hecaton_pairmask, hecaton_test); /*HecatonJump" + std::to_string(functionTag) +"*/" ;

	outstr = "if(hecaton_global_check < 0){\n" + label_str + "\n\t" + print_str + "\n\t"+ hf_str + "\n\t" + retStr + "\n" + outstr ;
        }else{//javad
	    outstr = "";
	}
	return outstr;

}

std::string GetStmtString(Stmt * stmt){
    std::string str;
    SourceLocation startloc = stmt->getBeginLoc();
    SourceLocation endloc = stmt->getEndLoc();
    if( startloc.isMacroID()){
        auto expansion_range = TheRewriter.getSourceMgr().getExpansionRange(startloc);
	startloc = expansion_range.getBegin();
    }
    if( endloc.isMacroID()){
        auto expansion_range = TheRewriter.getSourceMgr().getExpansionRange(endloc);
	endloc = expansion_range.getEnd();
    }
    SourceRange * range = new SourceRange(startloc,endloc);
    str = TheRewriter.getRewrittenText(*range);

    return str;
}
std::string GetStmtString2(Stmt * stmt){
    std::string str;
    SourceLocation startloc = stmt->getBeginLoc();
    SourceLocation endloc = stmt->getEndLoc().getLocWithOffset(1);
    if( startloc.isMacroID()){
        auto expansion_range = TheRewriter.getSourceMgr().getExpansionRange(startloc);
	startloc = expansion_range.getBegin();
    }
    if( endloc.isMacroID()){
        auto expansion_range = TheRewriter.getSourceMgr().getExpansionRange(endloc);
	endloc = expansion_range.getEnd();
	endloc = endloc.getLocWithOffset(1);
    }
    SourceRange * range = new SourceRange(startloc,endloc);
    str = TheRewriter.getRewrittenText(*range);

    return str;
}
void insert_clean_up_stirng(Function * myfunction ,std::string clstr){
     Stmt * body = myfunction->Body;
     Stmt * retSmt = NULL;
     
    for (Stmt::child_iterator i = body->child_begin(),
          		    e = body->child_end();
          		    i != e;
          		    ++i) {
        Stmt * currStmt = (*i);
	if(currStmt==NULL)
		continue;
        if(isa<ReturnStmt>(currStmt)){
		retSmt = currStmt;
	}	
    }
    SourceLocation loc = body->getEndLoc();
    if( retSmt != NULL){
	    loc = retSmt->getBeginLoc();
    }
    write_to_file2(loc,"//hecaton cleanup code\n" + clstr);
    

}
std::list<Pair_Candidate*> possible_pairs;
std::list<Pair_Candidate*>::iterator itpossible_pairs;
std::list<Pair_Candidate*> sorted_pairs;
std::list<Pair_Candidate*>::iterator itsorted_pairs;
void find_pairs(Function * myfunction){
    int index = 0;
    for( myfunction->itEHCs  = myfunction->EHCs.begin();
         myfunction->itEHCs != myfunction->EHCs.end();
         ++myfunction->itEHCs){
	    

        
	Stmt * start_stmt;
	Stmt * end_stmt;
	SourceLocation start_loc,end_loc;
	int start_line,end_line;
	if (index >=2){
            int index2= 0 ;
            std::list<EHC *>::iterator itEHCs2;
	    for( itEHCs2  = myfunction->EHCs.begin();
	         itEHCs2 != myfunction->EHCs.end();
		 ++itEHCs2){
		    start_stmt = (*itEHCs2)->head->headStmt;
		    if(index2 == index -2){
			    break;
		    }
		    index2 ++;

	    }
	    start_loc = start_stmt->getEndLoc ();

	}else{
		start_stmt = myfunction->Body;
	        start_loc = start_stmt->getBeginLoc();
	}
        start_line = mSM.getExpansionLineNumber(start_loc);


		
        
        EHC * ehc = (*myfunction->itEHCs);
	end_stmt = ehc->head->headStmt;
	end_loc = end_stmt->getBeginLoc();
        end_line = mSM.getExpansionLineNumber(end_loc);


        for( ehc->it_ehc_new_stmts  = ehc->ehc_new_stmts.begin();
	     ehc->it_ehc_new_stmts != ehc->ehc_new_stmts.end();
	     ++ehc->it_ehc_new_stmts ){
	    EHC_new_stmt *  ehcStmt = (*(ehc->it_ehc_new_stmts));
	    int pair_found = 0;	
            pair_found = recursive_pair_visit(myfunction->Body,ehcStmt,start_line,end_line);
	    ehcStmt->is_outer = 0;
	    if( pair_found){
		    ehcStmt->is_outer = 1;
	    }else{
    		    possible_pairs.clear();
                    recursive_probable_pair_visit(myfunction->Body,ehcStmt,start_line,end_line);
		    sorted_pairs.clear();
		    sort_pairs();
		    pick_the_most_probable(ehcStmt);
	    }
	}
        index ++;
    }

}
int recursive_pair_visit(Stmt * stmt, EHC_new_stmt * ehcStmt, int start_line, int end_line){
    int found = 0;
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	found = 0;
        if(currStmt!=NULL){
	    SourceLocation loc = currStmt->getBeginLoc();
	    int line = mSM.getExpansionLineNumber(loc);
	    if((line>start_line)&&(line<end_line)){
		if( is_pair(ehcStmt->stmt,currStmt) ){    
		    found = 1;
		    ehcStmt->pair = currStmt;
		    break;
		}
	    }
	    found = recursive_pair_visit(currStmt, ehcStmt, start_line,end_line);
        }
    }
    return found;
}
bool is_pair_outer(Stmt * stmt1, Stmt * stmt2, OuterEHC * outerEHC){

    if(is_pair(stmt1,stmt2)){
        return true;
    }
    return false;
}

std::vector<std::string> parse_param(std::string  str){
   std::vector<std::string> ret;
   str = remove_spaces(str);
   auto pos = str.find("=");
   if( pos != std::string::npos){
	auto param0 = str.substr(0,pos);   
	ret.push_back(param0);
   }else{
        ret.push_back("");
   }
   auto start_pos = str.find("(");
   auto end_pos = str.find_last_of(")");
   if ( (start_pos == std::string::npos) || 
	 (end_pos == std::string::npos) ){
       //it means failure and we should
       //check failure by the size of output vector
       //at this moment the size is one	   
       return ret;	   
   }
   std::string sub_str = str.substr(start_pos+1,end_pos-start_pos-1); 
   std::stringstream ss(sub_str);
   std::string token;
   while (std::getline(ss, token, ',')) {
       ret.push_back(token);
   }
   return ret;
}
std::string remove_spaces(std::string str ){
   std::string new_string = "";
   for (int i =0 ; i< str.length(); i++){
      char ch = str[i];
      if( (ch!=' ')&&(ch!='\n')&&(ch!='\t')){
          new_string = new_string + ch;
      }
   }
   return new_string;
}
bool is_pair(Stmt * stmt1, Stmt * stmt2){

    if (isa<CallExpr>(stmt1)){
          CallExpr * callexpr = cast<CallExpr>(stmt1);
          FunctionDecl * fncdcl = callexpr->getDirectCallee();
	  if(fncdcl != NULL){
              DeclarationName declname = fncdcl->getNameInfo().getName();
              std::string fncname = declname.getAsString();
	      if ( (fncname == "kfree") ||
	           (fncname == "kzfree") ) {
	          Expr * arg = callexpr->getArg(0);
	          std::string arg_str;
                  llvm::raw_string_ostream arg_stream(arg_str);
	          arg->printPretty(arg_stream, NULL, PrintingPolicy(LangOptions()));
	          arg_stream.flush();

	          if(isa<BinaryOperator>(stmt2)){
	              BinaryOperator * binop = cast<BinaryOperator>(stmt2);
	              if(binop->getOpcode() == BO_Assign ){
	                  Expr * lhs = binop->getLHS();
	                  Expr * rhs = binop->getRHS();
	                  std::string rhs_str;
	                  std::string lhs_str;
	                  llvm::raw_string_ostream rhs_stream(rhs_str);
	                  llvm::raw_string_ostream lhs_stream(lhs_str);

	                  lhs->printPretty(lhs_stream, NULL, PrintingPolicy(LangOptions()));
	                  rhs->printPretty(rhs_stream, NULL, PrintingPolicy(LangOptions()));
	                  rhs_stream.flush();
	                  lhs_stream.flush();
	                  if( (rhs_str.find("kzalloc") != std::string::npos)||
	            	  (rhs_str.find("kmalloc") != std::string::npos)){
	            	      if( lhs_str == arg_str){
	            	          return true;
	            	      }
	                  }

	              }
	          }
	          

	      }else if (fncname == "mutex_unlock"){
	          Expr * arg = callexpr->getArg(0);
	          std::string arg_str;
                  llvm::raw_string_ostream arg_stream(arg_str);
	          arg->printPretty(arg_stream, NULL, PrintingPolicy(LangOptions()));
	          arg_stream.flush();
	          if (isa<CallExpr>(stmt2)){
	              CallExpr * callexpr2 = cast<CallExpr>(stmt2); 
                      FunctionDecl * fncdcl2 = callexpr2->getDirectCallee();
		      if(fncdcl2 != NULL){ 
                          DeclarationName declname2 = fncdcl2->getNameInfo().getName();
                          std::string fncname2 = declname2.getAsString();
	                  if(fncname2 == "mutex_lock"){
	                      Expr * arg2 = callexpr2->getArg(0);
	                      std::string arg_str2;
                              llvm::raw_string_ostream arg_stream2(arg_str2);
	                      arg2->printPretty(arg_stream2, NULL, PrintingPolicy(LangOptions()));
	                      arg_stream2.flush();
	                      if( arg_str2 == arg_str ){
	                        return true;
	                      }

	                  }
		      }
	          }

	      } else if(fncname == "spin_unlock"){
	          Expr * arg = callexpr->getArg(0);
	          std::string arg_str;
                  llvm::raw_string_ostream arg_stream(arg_str);
	          arg->printPretty(arg_stream, NULL, PrintingPolicy(LangOptions()));
	          arg_stream.flush();
	          if (isa<CallExpr>(stmt2)){
	              CallExpr * callexpr2 = cast<CallExpr>(stmt2); 
                      FunctionDecl * fncdcl2 = callexpr2->getDirectCallee();
		      if(fncdcl2 != NULL){ 
                          DeclarationName declname2 = fncdcl2->getNameInfo().getName();
                          std::string fncname2 = declname2.getAsString();
	                  if((fncname2 == "spin_lock")||
				  (fncname2 =="_raw_spin_lock")){
	                      Expr * arg2 = callexpr2->getArg(0);
	                      std::string arg_str2;
                              llvm::raw_string_ostream arg_stream2(arg_str2);
	                      arg2->printPretty(arg_stream2, NULL, PrintingPolicy(LangOptions()));
	                      arg_stream2.flush();
	                      if( arg_str2 == arg_str ){
	                        return true;
	                      }

	                  }
		      }
	          }
	      } else if(fncname == "spin_unlock_irqrestore"){
	          Expr * arg = callexpr->getArg(0);
	          std::string arg_str;
                  llvm::raw_string_ostream arg_stream(arg_str);
	          arg->printPretty(arg_stream, NULL, PrintingPolicy(LangOptions()));
	          arg_stream.flush();
	          if (isa<CallExpr>(stmt2)){
	              CallExpr * callexpr2 = cast<CallExpr>(stmt2); 
                      FunctionDecl * fncdcl2 = callexpr2->getDirectCallee();
		      if(fncdcl2 != NULL){ 
                          DeclarationName declname2 = fncdcl2->getNameInfo().getName();
                          std::string fncname2 = declname2.getAsString();
	                  if((fncname2 == "spin_lock_irqsave")||
				  (fncname2 =="_raw_spin_lock_irqsave")){
	                      Expr * arg2 = callexpr2->getArg(0);
	                      std::string arg_str2;
                              llvm::raw_string_ostream arg_stream2(arg_str2);
	                      arg2->printPretty(arg_stream2, NULL, PrintingPolicy(LangOptions()));
	                      arg_stream2.flush();
	                      if( (arg_str2 == arg_str)||
			          (arg_str.find(arg_str2)!=std::string::npos)||
			          (arg_str2.find(arg_str)!=std::string::npos)
			       		){
				SourceLocation loc = stmt2->getEndLoc();
				if( loc.isMacroID() ) {
				   loc = TheRewriter.getSourceMgr().getExpansionRange(loc).getEnd();	
				}
				loc.dump(mSM);	
	                        return true;
	                      }

	                  }
		      }
	          }

	      }
	  }




    }
    return false;
}
void find_outer_ehcs(Function *  myfunction) {
    myfunction->outerEHCs.clear();	
    outer_ehcs_recurcive_visit(myfunction->Body, myfunction);
    handle_outer_ehcs_duplicates(myfunction);
    find_outer_ehcs_pairs(myfunction);
}

void find_outer_smss(Function *  myfunction) {
    myfunction->outerSMSs.clear();	
    outer_smss_recurcive_visit(myfunction->Body, myfunction);

}
void show(Function * myfunction){

    std::list<OuterEHC *>::iterator itouterEHCs;
    for( itouterEHCs  = myfunction->outerEHCs.begin();
         itouterEHCs != myfunction->outerEHCs.end();
         ++itouterEHCs){
	OuterEHC * outer1 = (*itouterEHCs);
	if(outer1->stmt == NULL)
	    continue;
        SourceLocation loc = outer1->stmt->getEndLoc();  
        write_to_file2(loc,"/*OPOPOP*/\n");
    }	
}
void find_outer_ehcs_pairs( Function *  myfunction){

    std::list<OuterEHC *>::iterator itouterEHCs;
    itouterEHCs = myfunction->outerEHCs.begin();
    while(itouterEHCs != myfunction->outerEHCs.end()){
	
	OuterEHC * outer1 = (*itouterEHCs);	 
	if(outer1->stmt == NULL)
	    continue;
	if (outer1->found_with_database){
	      outer_pair_database(myfunction , outer1); 	
	}else{
	      outer_pair_recurcive_visit(myfunction->Body, outer1->stmt, outer1 );
	}
        ++itouterEHCs;
    }

}
void  outer_pair_database(Function * myfunction, OuterEHC * outerEhc){
   find_all_possible_database_pairs(myfunction,outerEhc); 
   make_possible_database_pairs_unique(outerEhc);
   score_unique_possible_database_pairs(myfunction,outerEhc);
   find_most_possible_database_pair(myfunction,outerEhc);
   if ( outerEhc->pairs.begin() != outerEhc->pairs.end() ){
           auto pair = (*(outerEhc->pairs.begin()));
           add_all_duplicate_database_pairs(myfunction->Body,outerEhc,pair);
   }
   
	
}
void find_all_possible_database_pairs(Function * myfunction, OuterEHC * outerEhc){ 
    outerEhc->possible_pairs.clear();
    possible_database_pair_recurcive_visit(myfunction->Body, outerEhc);	
}
void  possible_database_pair_recurcive_visit(Stmt * base_stmt,OuterEHC * outerEhc){
	
    Stmt * stmt = base_stmt;	
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	if(currStmt==NULL)
		continue;
	if(is_possible_outer_pair(outerEhc,currStmt)){
	      outerEhc->possible_pairs.push_back(currStmt);
	}
	possible_database_pair_recurcive_visit(currStmt ,outerEhc);
    }
}
bool is_possible_outer_pair(OuterEHC * outerEHC,Stmt * stmt2){
    std::string fncname;
    if (isa<CallExpr>(stmt2)){
        CallExpr * callexpr2 = cast<CallExpr>(stmt2); 
        FunctionDecl * fncdcl2 = callexpr2->getDirectCallee();
        if(fncdcl2 != NULL){ 
             DeclarationName declname2 = fncdcl2->getNameInfo().getName();
             fncname = declname2.getAsString();
        }else{
             return false;
        }
    }else{
        return false;
    }
    std::list<int> pair_indexes = outerEHC->database_indexes;
    std::list<int>::iterator myit;
    for (myit = pair_indexes.begin();
        	    myit != pair_indexes.end();
        	    ++myit){
         int index = (*myit);   
	 if(ehc_fncs[index] == pair_fncs[index]){
	       std::string fncname2 = ehc_fncs[index];
	       auto param_list = parse_param(GetStmtString(stmt2));
	       bool is_ok = true;
	       if(fncname2 == "pinctrl_select_state"){
		  is_ok = false;
		  if(param_list.size() == 3){
		      if(param_list[2].find("active") != std::string::npos){
			  is_ok = true;
		       }
		  }
	       }
	       if(fncname2 == "msm_camera_config_vreg"){
		  is_ok = false;
		  if(param_list.size() == 8){
		      if(param_list[7] == "1" ){
			  is_ok = true;
		       }
		  }
	       }
	       if(fncname2 == "msm_camera_enable_vreg"){
		  is_ok = false;
		  if(param_list.size() == 8){
		      if(param_list[7] == "1" ){
			  is_ok = true;
		       }
		  }
	       }
	       if(fncname2 == "regulator_set_voltage"){
		  is_ok = false;
		  if(param_list.size() == 4){
		      if(param_list[2] == "1" ){
			  is_ok = true;
		       }
		  }
	       }

	       if(is_ok){
		  outerEHC->possible_pairs.push_back(stmt2);     
	       }	       
	 }else{
            if(fncname == pair_fncs[index]){
		  outerEHC->possible_pairs.push_back(stmt2);  
            } 
	 }
    }
    return false;
}

void make_possible_database_pairs_unique(OuterEHC * outerEhc){
   outerEhc->unique_possible_pairs.clear();	
   for(auto it = outerEhc->possible_pairs.begin();
		   it != outerEhc->possible_pairs.end();
		   ++it){
       bool is_unique = true;
       Stmt * stmt1 = (*it);
       if (stmt1 == NULL){
          continue;
       }
       for (auto it2 = outerEhc->unique_possible_pairs.begin();
		it2 !=  outerEhc->unique_possible_pairs.end();
	        ++it2){
	    ScoredStmt * Sstmt2 = (*it2);   
	    Stmt * stmt2 = Sstmt2->stmt;
	    if( stmt2 == NULL ){
	       std::string stmt_string1;
	       std::string stmt_string2;
	       stmt_string1 = GetStmtString(stmt1);
	       stmt_string2 = GetStmtString(stmt2);

	       if(stmt_string2 == stmt_string1){
	           is_unique = false;
	       }
	    }  
       }
       if(is_unique){
	    ScoredStmt * mySstmt = new ScoredStmt();
	    mySstmt->stmt = stmt1;   
            outerEhc->unique_possible_pairs.push_back(mySstmt);
       }       
       
   }
}
void  score_unique_possible_database_pairs(Function * myfunction, OuterEHC * outerEHC ){
	int score = 0;
	for (auto it = outerEHC->unique_possible_pairs.begin();
			it != outerEHC->unique_possible_pairs.end();
			++it){
	     score = 0;	
	     ScoredStmt * Sstmt = (*it);
             Stmt * currStmt = Sstmt->stmt;		
	     Stmt * ehc_stmt = outerEHC->stmt;
	     auto param_list_pair = parse_param(GetStmtString(currStmt));
	     auto param_list_ehc = parse_param(GetStmtString(ehc_stmt)); 
	     //special case of kfree
	     // ptr = kfree;
	     // pair(....ptr) is showing being pair
	     if ( GetStmtString(ehc_stmt).find("kfree") != std::string::npos){
                Stmt * parent =  find_parent_recursive( currStmt , myfunction->Body);
		if(parent != NULL){
		   parent = find_parent_recursive(parent, myfunction->Body);
		   if( parent != NULL){
		       param_list_pair = parse_param(GetStmtString(parent));
		   }
		}
	        if (param_list_ehc[1] == param_list_pair[0]){
		    if(param_list_pair[0] != "")	
	                score +=100;
	        }
	     }
	     Stmt * parent = find_parent_recursive( currStmt,myfunction->Body);
	     if( parent != NULL){
	         if(isa<BinaryOperator>(parent)){
		       param_list_pair = parse_param(GetStmtString(parent));
		 }else{
		     parent = find_parent_recursive( parent , myfunction->Body);
		     if ( parent != NULL ){
		        if( isa<BinaryOperator>(parent)){
		            param_list_pair = parse_param(GetStmtString(parent));
			}
		     }
		 }
	     
	     } 
	     // as a generic rule if 
	     // the two pairs have any
	     // parameter in common it is a good point
             for( int i = 0 ; i< param_list_pair.size() ;i++){
                  for( int j = 0 ; j< param_list_ehc.size() ;j++){
		        if(param_list_ehc[j]==param_list_pair[i]){
			    if(param_list_pair[i] != "")	
			    	score += 10;
			}
		  }
	     }
	     Sstmt->score = score; 
	}
}
void find_most_possible_database_pair(Function * myfunction,OuterEHC* outerEHC){
        Stmt * max_stmt = NULL;
	int max_score = 0;
	for (auto it = outerEHC->unique_possible_pairs.begin();
			it != outerEHC->unique_possible_pairs.end();
			++it){
	     ScoredStmt * Sstmt = (*it);
	     if(Sstmt == NULL){
		  continue;
	     }
	     if (Sstmt->score >=  max_score){
	          max_score = Sstmt->score;
		  max_stmt = Sstmt->stmt;
	     }	
	}
	if(max_stmt != NULL){
	    outerEHC->pairs.push_back(max_stmt);
        }
}

void  add_all_duplicate_database_pairs(Stmt * Base, OuterEHC * outerEhc , Stmt* pair){
      for ( auto i = Base->child_begin(), e = Base->child_end();
		      i!=e;
		      ++i){
            Stmt * currStmt = (*i);
	    if(currStmt == NULL){
	       continue;
	    }
	    std::string stmt_string1;
	    std::string stmt_string2;
	    llvm::raw_string_ostream stream1(stmt_string1);
	    llvm::raw_string_ostream stream2(stmt_string2);
	    pair->printPretty(stream1, NULL, PrintingPolicy(LangOptions()));
	    currStmt->printPretty(stream2, NULL, PrintingPolicy(LangOptions()));
	    stream1.flush();
	    stream2.flush();

	    if(stmt_string2 == stmt_string1){
		if(recursiveGetEnd(currStmt) != recursiveGetEnd(pair)){    
	            outerEhc->pairs.push_back(currStmt);
		}
	    }

	   add_all_duplicate_database_pairs(currStmt,outerEhc, pair);
      }

}
void  outer_pair_recurcive_visit(Stmt * base_stmt, Stmt * outerStmt , OuterEHC * outerEhc){
    Stmt * stmt = base_stmt;	
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	if(currStmt==NULL)
		continue;
        if(currStmt!=NULL){
	    if(is_pair_outer(outerStmt,currStmt,outerEhc)){
	        outerEhc->pairs.push_back(currStmt);
	    }else{	    
	        outer_pair_recurcive_visit(currStmt , outerStmt ,outerEhc);
	    }
        }
    }
    return;
}
void  outer_ehcs_recurcive_visit(Stmt * stmt , Function * myfunction ){
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	if(currStmt==NULL)
		continue;
        if(currStmt!=NULL){
            if (isa<CallExpr>(currStmt)){
                  CallExpr * callexpr = cast<CallExpr>(currStmt);
                  FunctionDecl * fncdcl = callexpr->getDirectCallee();
	          if(fncdcl != NULL){
                      DeclarationName declname = fncdcl->getNameInfo().getName();
                      std::string fncname = declname.getAsString();
		      if ((fncname == "mutex_unlock")
			 || (fncname == "spin_unlock")
			 || (fncname == "spin_unlock_irqrestore")){
	             if(is_in_macro(currStmt,myfunction->Body)){
	             }else{
    		          SourceLocation loc = currStmt->getEndLoc();  
		      	  OuterEHC * ehcnew = new OuterEHC();
			  ehcnew->stmt = currStmt;
			  myfunction->outerEHCs.push_back(ehcnew);	  
		      }
		     }else{
			   std::list<int> founds =
			      find_fnc(ehc_fncs, DATABASE_SIZE, fncname);  
			    if(!(founds.empty())){
			   	OuterEHC * ehcnew = new OuterEHC();
			       auto param_list = parse_param(GetStmtString(currStmt));
			       bool is_ok = true;
			       if(fncname == "pinctrl_select_state"){
			          is_ok = false;
				  if(param_list.size() == 3){
				      if(param_list[2].find("suspend") != std::string::npos){
				          is_ok = true;
				       }
				  }
			       }
			       if(fncname == "msm_camera_config_vreg"){
			          is_ok = false;
				  if(param_list.size() == 8){
				      if(param_list[7] == "0" ){
				          is_ok = true;
				       }
				  }
			       }
			       if(fncname == "msm_camera_enable_vreg"){
			          is_ok = false;
				  if(param_list.size() == 8){
				      if(param_list[7] == "0" ){
				          is_ok = true;
				       }
				  }
			       }
			       if(fncname == "regulator_set_voltage"){
			          is_ok = false;
				  if(param_list.size() == 4){
				      if(param_list[2] == "0" ){
				          is_ok = true;
				       }
				  }
			       }
			       if(fncname == "platform_set_drvdata"){
			          is_ok = false;
			       }

			       if(is_in_macro(currStmt,myfunction->Body)){
			           is_ok = false;
			       }

			       if(is_ok){
			           ehcnew->stmt = currStmt;
			           ehcnew->found_with_database = true;
			           ehcnew->database_indexes = founds;
			           myfunction->outerEHCs.push_back(ehcnew);
			       }	       
		           }
		     }
	          }
	    }
	    outer_ehcs_recurcive_visit(currStmt , myfunction);
        }
    }
    return;
}
void  outer_smss_recurcive_visit(Stmt * stmt , Function * myfunction ){
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
	int unique = 1;    
        Stmt *currStmt = (*i);
	if(currStmt==NULL)
		continue;
        if(currStmt!=NULL){
            if (isa<CallExpr>(currStmt)){
                  CallExpr * callexpr = cast<CallExpr>(currStmt);
                  FunctionDecl * fncdcl = callexpr->getDirectCallee();
	          if(fncdcl != NULL){
                      DeclarationName declname = fncdcl->getNameInfo().getName();
                      std::string fncname = declname.getAsString();
		      std::list<int> founds =  find_fnc(pair_fncs, DATABASE_SIZE, fncname);  
		      if(!(founds.empty())){
		           OuterSMS * smsnew = new OuterSMS();
		           smsnew->stmt = currStmt;
		           smsnew->fnc_name = fncname;
		           smsnew->database_indexes = founds;
			   unique = 1;
			   for( myfunction->itouterSMSs = myfunction->outerSMSs.begin();
					   myfunction->itouterSMSs != myfunction->outerSMSs.end();
					   ++myfunction->itouterSMSs){
				   OuterSMS * outer1 = (*myfunction->itouterSMSs);
			   	   std::string name1 = outer1->fnc_name;
			           if( fncname == name1){
					unique = 0;
			     		break;
				   }		
			   }
			   if( unique == 1){
		           	myfunction->outerSMSs.push_back(smsnew);
			   }
		              
		     }
	          }
	    }
	    outer_smss_recurcive_visit(currStmt , myfunction);
        }
    }
    return;
}
void  score_missing_statements( Function * myfunction ){
	   for( myfunction->itouterSMSs = myfunction->outerSMSs.begin();
			   myfunction->itouterSMSs != myfunction->outerSMSs.end();
			   ++myfunction->itouterSMSs){
		   OuterSMS * outer_sms = (*myfunction->itouterSMSs);	
		   std::list<int> sms_name_list = outer_sms->database_indexes;
		   int pair_found = 0 ;
		   for( myfunction->itouterEHCs = myfunction->outerEHCs.begin();
				   myfunction->itouterEHCs != myfunction->outerEHCs.end();
				   ++myfunction->itouterEHCs){
			   OuterEHC * outer_ehc = (*myfunction->itouterEHCs);	
			   std::list<int> ehc_name_list = outer_ehc->database_indexes;
		           std::list<int>::iterator smsit;
		           std::list<int>::iterator ehcit;
		           for (smsit = sms_name_list.begin();
		                	    smsit != sms_name_list.end();
		                	    ++smsit){
		                 int sms_index = (*smsit);
				 for (ehcit = ehc_name_list.begin();
						    ehcit != ehc_name_list.end();
						    ++ehcit){
					 int ehc_index = (*ehcit);
					 if( ehc_index == sms_index ){
						 pair_found = 1;
						 break;
					 }
				 }
				 if(pair_found == 1){
					break;
			         }		
			   }	 
			   if(pair_found == 1){
				break;
			   }		
		   }
		   if(pair_found == 0){
			if(myfunction->confidence_score.init_flag != 1399){
		   		myfunction->confidence_score.score_missing_ehc = -SCORE_MISSING_EHC; 
				myfunction->confidence_score.init_flag = 1399;
			}else{
		   		myfunction->confidence_score.score_missing_ehc -= SCORE_MISSING_EHC; 
			}
		   }

	   }
}
void  handle_outer_ehcs_duplicates(Function * myfunction){
    std::list<OuterEHC *>::iterator itouterEHCs;
    std::list<OuterEHC *>::iterator itouterEHCs2;
    itouterEHCs = myfunction->outerEHCs.begin();
    while(itouterEHCs != myfunction->outerEHCs.end()){
	
	OuterEHC * outer1 = (*itouterEHCs);	 
	if(outer1->stmt == NULL)
	    continue;

    	itouterEHCs2 = itouterEHCs;
	++itouterEHCs2;
	while(itouterEHCs2 != myfunction->outerEHCs.end()){
            
	    OuterEHC * outer2 = (*itouterEHCs2);
	    if(outer2->stmt==NULL)
		continue;
	    int is_duplicate = 0;
	    std::string stmt_string1;
	    std::string stmt_string2;
	    stmt_string1 = GetStmtString(outer1->stmt);
	    stmt_string2 = GetStmtString(outer2->stmt);

	    if(stmt_string2 == stmt_string1){
	        is_duplicate = 1;
	    }

	    if (is_duplicate == 1){
	        outer1->duplicates.push_back(outer2->stmt);
	        itouterEHCs2 = myfunction->outerEHCs.erase(itouterEHCs2);
	    }else{
	        ++itouterEHCs2;	
	    }	
	}
	for( myfunction->itEHCs  = myfunction->EHCs.begin();
	     myfunction->itEHCs != myfunction->EHCs.end();
	     ++myfunction->itEHCs){

		EHC * ehc = (*myfunction->itEHCs);
		if(ehc==NULL)
		    continue;
		for( ehc->it_ehc_new_stmts  = ehc->ehc_new_stmts.begin();
		     ehc->it_ehc_new_stmts != ehc->ehc_new_stmts.end();
     		   ++ehc->it_ehc_new_stmts){	
			EHC_new_stmt * newstmt = (*ehc->it_ehc_new_stmts);
			if(newstmt==NULL)
			    continue;
	                std::string stmt_string1;
	                std::string stmt_string2;
	    		stmt_string1 = GetStmtString(outer1->stmt);
	    		stmt_string2 = GetStmtString(newstmt->stmt);

	              if(stmt_string2 == stmt_string1){
		            newstmt->is_outer = 1;
			    //we might want to use the inner duplicate
			    //to locate the pair of the ehc better
			    outer1->has_inner_duplicate = true;
			    outer1->inner_duplicate = newstmt->stmt;
	              }
		      if(GetStmtString(outer1->stmt) == GetStmtString(newstmt->stmt)){
			    outer1->has_inner_duplicate = true;
			    outer1->inner_duplicate = newstmt->stmt;
		      }
        


		}
	}
        ++itouterEHCs;
    }
}
void recursive_probable_pair_visit(Stmt * stmt, EHC_new_stmt * ehcStmt, int start_line, int end_line){
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	if(currStmt==NULL)//FIXME
		continue;
	bool pair_candidate=false;
        if(currStmt!=NULL){
	    SourceLocation loc = currStmt->getBeginLoc();
	    int line = mSM.getExpansionLineNumber(loc);
	    if((line>start_line)&&(line<end_line)){
		if (isa<BinaryOperator>(currStmt)||
			isa<CallExpr>(currStmt) ||
			isa<UnaryOperator>(currStmt)||
			(isa<IfStmt>(currStmt) && isa<IfStmt>(ehcStmt->stmt))
				){
		         if(isa<BinaryOperator>(currStmt)){
		           	BinaryOperator *binop = cast<BinaryOperator>(currStmt);
				if(binop->isAssignmentOp()){
					pair_candidate = true;
				}
			 }else{
				pair_candidate = true;
			 }
		}
	    }
	    if(pair_candidate == true){
		    Pair_Candidate * possible_pair = new Pair_Candidate();
		    possible_pair->stmt = ehcStmt->stmt;
		    possible_pair->pair = currStmt;
		    possible_pair->probability = pair_evaluation(ehcStmt->stmt,currStmt);
		    possible_pairs.push_back(possible_pair);
	    }
	    recursive_probable_pair_visit(currStmt, ehcStmt, start_line,end_line);
        }
    }
    return;
}
int pair_evaluation( Stmt * stmt1 , Stmt * stmt2){

    std::string stmt_string1;
    std::string stmt_string2;
    llvm::raw_string_ostream stream1(stmt_string1);
    llvm::raw_string_ostream stream2(stmt_string2);

    stmt1->printPretty(stream1, NULL, PrintingPolicy(LangOptions()));
    stmt2->printPretty(stream2, NULL, PrintingPolicy(LangOptions()));
    stream1.flush();
    stream2.flush();

    int score;
    int len = stmt_string1.length();
    score = compare (&stmt_string1,&stmt_string2, 0); 

    int normalized_score = (score*100) /len;
    return normalized_score;

}
int compare (std::string * strptr1, std::string * strptr2, int score){
   std::string string1 = *strptr1;
   std::string string2 = *strptr2;   
   int sub_len;
   sub_len = delete_longest_common_substr(strptr1,strptr2);
   if (sub_len != -1 ){
	   return compare(strptr1 , strptr2 , score + sub_len);
   }else{
	   return score;
   }
}
int delete_longest_common_substr(std::string * strptr1,std::string * strptr2){
   std::string sub_str;
   int len = strptr1->length();
   
   for (int sublen =len ; sublen >= 3 ; sublen--){
       for (int i =0 ; i<=len-sublen ; i++){
           sub_str = strptr1->substr( i , sublen);
	   std::size_t found = strptr2->find(sub_str);
	   if (found!=std::string::npos){
	       strptr2->erase(found,sub_str.length());
	       strptr1->erase(i,sublen);
	       return sublen;
	      
	   }

       }	       
	
   }
   return -1;
}
void sort_pairs(){
    int max_score = 0;	
    std::list<Pair_Candidate*>::iterator it_max_possible_pairs;
    if( possible_pairs.empty() ){
	    return;
    }
    it_max_possible_pairs = possible_pairs.begin();
    for (itpossible_pairs = possible_pairs.begin() ; 
         itpossible_pairs != possible_pairs.end() ;
        ++itpossible_pairs){
	Pair_Candidate * possible_pair = (* itpossible_pairs);    
	int score = possible_pair->probability;
	if (score>max_score){
		max_score = score;
		it_max_possible_pairs = itpossible_pairs;
	}
    }
    sorted_pairs.push_back((*it_max_possible_pairs));
    possible_pairs.erase(it_max_possible_pairs);
    sort_pairs();
    return;
}
void pick_the_most_probable( EHC_new_stmt * ehcStmt){
	itsorted_pairs = sorted_pairs.begin();
	Pair_Candidate * first_pair = (*itsorted_pairs);
	if(first_pair!=NULL){
	    ehcStmt->pair = first_pair->pair;
	}else{
	    ehcStmt->pair=NULL;
	}
}
void insert_pairmask_decl( Function * myfunction){
    if (isa<CompoundStmt>(myfunction->Body)){
        CompoundStmt * cmpnd_stmt = cast<CompoundStmt>(myfunction->Body);
	SourceLocation ST2 = cmpnd_stmt->getLBracLoc();
	SourceLocation ST = Lexer::getLocForEndOfToken(ST2,0,
		mSM,LangOptions() );	
	if(mSM.isInMainFile(ST)){	
	    write_to_file2(ST, "\n\t/*Hecaton pair mask flag*/\n"
				"\tuint64_t hecaton_pairmask = 0;\n");
	    write_to_file2(ST, "\n\t/*Hecaton return*/\n"
				"\tint hecaton_test = 0xdeadbeaf;\n");
	}
	return;

    }	    
}
void assign_pairmask_id (Function * myfunction){
	uint64_t id = 0x01;
	int index = 0;
	for( myfunction->itEHCs  = myfunction->EHCs.begin();
	     myfunction->itEHCs != myfunction->EHCs.end();
	     ++myfunction->itEHCs){

		EHC * ehc = (*myfunction->itEHCs);	
		for( ehc->it_ehc_new_stmts  = ehc->ehc_new_stmts.begin();
		     ehc->it_ehc_new_stmts != ehc->ehc_new_stmts.end();
     		   ++ehc->it_ehc_new_stmts){	
			EHC_new_stmt * newstmt = (*ehc->it_ehc_new_stmts);
			if (newstmt->is_outer == 0){
		            newstmt->pairmask_id = id;
			    id = id << 1;
			    index ++;
			}	
		}
	}
	for( myfunction->itouterEHCs  = myfunction->outerEHCs.begin();
	     myfunction->itouterEHCs != myfunction->outerEHCs.end();
	     ++myfunction->itouterEHCs){

		OuterEHC * outerEhc = (*myfunction->itouterEHCs);	
		outerEhc->pairmask_id = id;
		id = id << 1;
		index ++;
		
	}
	if( index > 5){
		myfunction->confidence_score.score_number_ehc = -SCORE_NUMBER_EHC;
	}	
	if( index > 8){
		myfunction->confidence_score.score_number_ehc = -2 * SCORE_NUMBER_EHC;
	}	

}

void insert_pairmask_stamps(Function * myfunction){
	uint64_t id;
	std::string stamp;
	for( myfunction->itEHCs  = myfunction->EHCs.begin();
	     myfunction->itEHCs != myfunction->EHCs.end();
	     ++myfunction->itEHCs){

		EHC * ehc = (*myfunction->itEHCs);	
		for( ehc->it_ehc_new_stmts  = ehc->ehc_new_stmts.begin();
		     ehc->it_ehc_new_stmts != ehc->ehc_new_stmts.end();
     		   ++ehc->it_ehc_new_stmts){	
			EHC_new_stmt * newstmt = (*ehc->it_ehc_new_stmts);
			if(newstmt->is_outer !=0)
			   continue;
			Stmt * pair = newstmt->pair;
			if(pair==NULL)
			   continue;
			SourceLocation loca = find_proper_loc_to_insert_pairmask_stamp(pair, myfunction->Body);
		        id = newstmt->pairmask_id;
			std::ostringstream o;
			o<<id;
			stamp =" hecaton_set_bit(hecaton_pairmask, " 
				+ o.str() + ");";
			if (loca.isValid())	
				write_to_file3(loca,stamp);
			
		}
	}
	for( myfunction->itouterEHCs  = myfunction->outerEHCs.begin();
	     myfunction->itouterEHCs != myfunction->outerEHCs.end();
	     ++myfunction->itouterEHCs){

		OuterEHC * outer1 = (*myfunction->itouterEHCs);	
		Stmt * pair = outer1->stmt;

	        if( pair == NULL){
	            continue;
	        }
	        SourceLocation loca = find_proper_loc_to_insert_pairmask_stamp(pair,myfunction->Body);
	        id = outer1->pairmask_id;
	        std::ostringstream o;
	        o<<id;
	        stamp =" hecaton_unset_bit(hecaton_pairmask, " 
	            + o.str() + ");";
	        
		if (loca.isValid())	
	        	write_to_file3(loca,stamp);
	        for (outer1->itduplicates = outer1->duplicates.begin();
	        	outer1->itduplicates != outer1->duplicates.end();
	        	++outer1->itduplicates){
	            Stmt * pair = (*(outer1->itduplicates));
    	            if( pair == NULL){
	        	continue;
    	            }
		    SourceLocation loca = find_proper_loc_to_insert_pairmask_stamp(pair,myfunction->Body);
		    id = outer1->pairmask_id;
		    std::ostringstream o;
		    o<<id;
		    stamp =" hecaton_unset_bit(hecaton_pairmask, " 
			+ o.str() + ");";
		    
		    if (loca.isValid())	
                    	write_to_file3(loca,stamp);
	            	    
	        }
	        for (outer1->itpairs = outer1->pairs.begin();
	       		outer1->itpairs != outer1->pairs.end();
	       		++outer1->itpairs){
	            Stmt * pair = (*(outer1->itpairs));
    	            if( pair == NULL){
	       		continue;
    	            }
		    SourceLocation loca = find_proper_loc_to_insert_pairmask_stamp(pair,myfunction->Body);
	            id = outer1->pairmask_id;
	            std::ostringstream o;
	            o<<id;
		    stamp =" hecaton_set_bit(hecaton_pairmask, " 
			+ o.str() + ");";
		   if (loca.isValid())	
                   	write_to_file3(loca,stamp);
	           	    
	       }
		
	}
}

SourceLocation find_proper_loc_to_insert_pairmask_stamp(Stmt * pair,Stmt* Body){


	Stmt * parent = find_ifstmt_grandparent_recursive(pair, Body);
	SourceLocation loca;
	if( stmt_in_parent_condintion(pair,parent)){
	    Stmt * parentBody;
	    if(isa<IfStmt>(parent)){  
		parentBody = cast<IfStmt>(parent)->getThen();  
	    }
	    if(isa<ForStmt>(parent)){
		parentBody = cast<ForStmt>(parent)->getBody();    
	    }
	    if(isa<WhileStmt>(parent)){
		parentBody = cast<WhileStmt>(parent)->getBody();    
	    }
	    if(isa<CompoundStmt>(parentBody)){
		CompoundStmt * parentCompBody = cast<CompoundStmt>(parentBody);
		loca = parentCompBody->getLBracLoc().getLocWithOffset(1);
	    }else{
	    }

	}else if ( isa<IfStmt>(pair) ||
			isa<ForStmt>(pair) ||
			isa<WhileStmt>(pair) ){
	    if(isa<CompoundStmt>(pair)){
		CompoundStmt * pairCompBody = cast<CompoundStmt>(pair);
		loca = pairCompBody->getRBracLoc().getLocWithOffset(1);
	    }else{
	    }
	} else {
	    loca = recursiveGetEnd(pair); 	
	}
	return loca;
}
SourceLocation recursiveGetStart(Stmt *stmt){
  
   SourceLocation startLoc = stmt->getBeginLoc();
   if( startLoc.isMacroID() ) {
    // Get the start/end expansion locations
    auto expansionRange = 
   	  mSM.getExpansionRange( startLoc );
   
    // We're just interested in the start location
    startLoc = expansionRange.getBegin();
   }
   return startLoc;

}
SourceLocation recursiveGetEnd(Stmt *stmt){  
   SourceLocation endloc1 = stmt->getEndLoc();
   if( endloc1.isMacroID() ) {
    // Get the start/end expansion locations
    auto expansionRange = 
   	  mSM.getExpansionRange( endloc1 );
   
    // We're just interested in the start location
    endloc1 = expansionRange.getEnd();
   }
   SourceLocation loc = Lexer::getLocForEndOfToken(endloc1, 0, mSM, LangOptions());
   if (!loc.isValid()){
      return stmt->getEndLoc();
   }else{
      SourceLocation loc2 = Lexer::getLocForEndOfToken(loc, 0, mSM, LangOptions());
      if (!loc2.isValid()){
         return loc2;
      }else{
         loc = loc2;
      }
   }
   return loc;

}
Stmt *  find_ifstmt_grandparent_recursive(Stmt *stmt, Stmt *Body){
    Stmt * parent =  find_parent_recursive( stmt , Body);
    while (parent){
        if((parent==NULL) || (Body==NULL)){
	    return NULL;
	}
        if( parent->getBeginLoc() == Body->getBeginLoc() ){
             return NULL;
	}
	if( isa<IfStmt>(parent) ){
	     return parent;
	}
	if( isa<ForStmt>(parent) ){
		return parent;
	}
	if (isa<WhileStmt>(parent)){
		return parent;
	}
	parent = find_parent_recursive( parent, Body);

    }
    return NULL;
}
Stmt * find_parent_recursive (Stmt *stmt, Stmt *Body) {
  Stmt * parent=NULL;
  for (Stmt::child_iterator i = Body->child_begin(), e = Body->child_end();
	      i != e;
	      ++i) {
     Stmt *currBody = (*i);
     if(currBody==NULL){
        continue;
     }
     if(currBody == stmt){
	     return Body;
     }
     parent = find_parent_recursive(stmt , currBody);
     if(parent != NULL){
         return parent;	     
     }
  }
  return parent;
}
bool stmt_in_parent_condintion( Stmt * pair, Stmt * parent){
    if(parent == NULL){
       return false;
    }       
    std::string parent_cond_str = "";	
    std::string pair_str = GetStmtString(pair);
    if( isa<IfStmt>(parent) ||
		    isa<ForStmt>(parent)||
		    isa<WhileStmt>(parent) ){
        if(isa<IfStmt>(parent)){
	    IfStmt * ifparent = cast<IfStmt>(parent);
	    Stmt * cond = ifparent->getCond();
	    parent_cond_str = GetStmtString(cond);
	}
	if(isa<ForStmt>(parent)){
	   ForStmt * forparent = cast<ForStmt>(parent);
	   Stmt * cond = forparent->getInit();
	   if(cond)
	   	parent_cond_str = GetStmtString(cond);
	   cond = forparent->getCond();
	   if(cond)
	   	parent_cond_str += " ; " + GetStmtString(cond);
	   cond = forparent->getInc();
	   if(cond)
	   	parent_cond_str += " ; " + GetStmtString(cond);

	}
        if(isa<WhileStmt>(parent)){
	    WhileStmt * whileparent = cast<WhileStmt>(parent);
	    Stmt * cond = whileparent->getCond();
	    parent_cond_str = GetStmtString(cond);
	}
	std::size_t found = parent_cond_str.find(pair_str);
        if (found!=std::string::npos){
	     return true;
	}
    }else{
        return false;
    }
    return false;
}

std::list<int> find_fnc(std::string * str_arr , int size , std::string mystr){
   std::list<int> found_indices;
   found_indices.clear();
   for (int i = 0 ; i<size ; i++){
       if( mystr == str_arr[i]){
          found_indices.push_back(i);
       }
   }
   return found_indices;

}
private:
  Rewriter &TheRewriter;
};
class HecatonConsumer : public ASTConsumer {
  CompilerInstance &Instance;

public:

  HecatonConsumer(CompilerInstance &Instance, Rewriter &R)
      : Instance(Instance), hecaton_visitor(Instance,R) {}
  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i) {
    // Traverse the declaration using our AST visitor
	hecaton_visitor.TraverseDecl(*i);
    }

    return true;
  }
private:
  HecatonASTVisitor hecaton_visitor;	

};

class HecatonAction : public PluginASTAction {
public:
   std::string generate_file_name(std::string path){
	   path = "XXXPATHXXX" + path;
	   llvm::errs() << path<<"\n"; 
           return path;
   }

  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    llvm::errs() << "\n\n\n** Hecaton Pass2 **\n\n\n ";
    llvm::errs() << "** EndSourceFileAction for: "
                 << SM.getFileEntryForID(SM.getMainFileID())->getName().str() << "\n";
    std::string main_file_str = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
    std::string new_file_str = generate_file_name(main_file_str);
    std::error_code error_code;
    llvm::errs() << "New file name: \n"<<new_file_str<<"\n";
    llvm::raw_fd_ostream outFile(new_file_str, error_code, llvm::sys::fs::F_None);
    // Now emit the rewritten buffer.
    TheRewriter.getEditBuffer(SM.getMainFileID()).write(outFile);
    outFile.close();
  }

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts()); 
    return std::make_unique<HecatonConsumer>(CI,TheRewriter);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    if (!args.empty() && args[0] == "help")
      PrintHelp(llvm::errs());

    return true;
  }
  void PrintHelp(llvm::raw_ostream& ros) {
    ros << "Help for hecaton-pass2 plugin\n";
  }
private:
  Rewriter TheRewriter;
};

}

static FrontendPluginRegistry::Add<HecatonAction>
X("hecaton-pass2", "print hecaton function");
