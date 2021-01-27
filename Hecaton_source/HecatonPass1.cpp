//===- HecatonPass1.cpp ---------------------------------------------===//
//
// This is the first pass of Hecaton which prepare files for bowknot insertion
// 
//
//===----------------------------------------------------------------------===//

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
#include "fncs_header.h"
#include "clang/Basic/FileManager.h"
using namespace clang;

namespace {


#define write_to_file(loc,str) TheRewriter.InsertText(loc,str,true,true)
#define write_to_file2(loc,str) TheRewriter.InsertText(loc,str,true,true)
#define write_to_file3(loc,str) TheRewriter.InsertText(loc,str,true,true)


class varDecl{
	public:
		std::string str;
		std::string name;
		std::string old_name;
		std::string type;
		Stmt * stmt;
		Decl * decl;



};
std::list<varDecl*> varDecls;
std::list<varDecl*> fncDecls;
std::list<varDecl*>::iterator itvarDecls;

std::list<Stmt *> commentedVars;
std::list<Stmt *>::iterator itcommentedVars;

class Function{
	public:
		Stmt * Body;
		std::string typeStr;
		std::string name;
		bool is_closing;

};
std::list<Function*> listOfFunctions;
std::list<Function*>::iterator itFunctions;
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
    if (f->hasBody()) {
      Stmt *FuncBody = f->getBody();
      // Function name
      DeclarationName DeclName = f->getNameInfo().getName();
      std::string FuncName = DeclName.getAsString();
      if(!mSM.isInMainFile(FuncBody->getBeginLoc())){
	    return true;
      }
     	
      if(FuncBody->getBeginLoc().isMacroID()){
	   return true;
      }
      Function * myfunction = new Function();
      myfunction->Body = FuncBody;
      myfunction->name = FuncName;
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
      //llvm::errs()<<"Function name is: "<<FuncName<<"\n";
      listOfFunctions.push_back(myfunction);
  // Hecaton: Convert all of the single statement if, while and for to compound statemetns with {} 
      recursiveVisit(FuncBody);


  // Hecaton: Move all the Variable declerations to the begining of the function 
  // change the name of the variable if there is a duplicate
  // and use the changed name in the first scope of decleration.
      var_suffix = 1;
      move_var_decl(FuncBody);
      insert_var_decl(FuncBody);
      replace_duplicate_vars(FuncBody);
      seperate_decl_init(FuncBody);

    }  

    return true;
  }
 
  void recursiveVisit (Stmt *stmt) {
      for (Stmt::child_iterator i = stmt->child_begin(), e = stmt->child_end();
		      i != e;
		      ++i) {
          Stmt *currStmt = (*i);
          if(currStmt!=NULL){
          if (isa<IfStmt>(currStmt)){		
              IfStmt *IfStatement = cast<IfStmt>(currStmt);
              Stmt *Then = IfStatement->getThen();
	      Stmt *Else = IfStatement->getElse();
	      if(currStmt->getBeginLoc().isMacroID()){
		  //continue;
	      }else{
	          //llvm::errs()<<"ss[11]\n";
                  
                  if(isa<CompoundStmt>(Then)){
	          }else{
	             //write_to_file(Then->getBeginLoc(),"\n//'Then' Single start\n{\n");

	             //llvm::errs()<<"UIUI:\t"<<GetStmtString(currStmt);
	             write_to_file( recursiveGetStart(Then),"\n//'Then' Single start\n{\n");
	             write_to_file( recursiveGetEnd(Then),"\n}\n// 'Then' Single end\n");

	             //write_to_file(Then->getBeginLoc(),"{\n");
	             //write_to_file( recursiveGetEnd(Then),"\n}\n");
	          }
	          if(Else){
                      if(isa<CompoundStmt>(Else)){
	              }else{
	                 //to role-out else if statements
	                  Stmt::child_iterator j = Else->child_begin();
	                  Stmt * else_first_child;
	                  bool is_ifelse = false;
	                  if( j != Else->child_end()){
	                       else_first_child = (*j);
	                       if(else_first_child!=NULL){
	                           if(isa<IfStmt>(else_first_child)){
	            	           is_ifelse =true;
	            	       }
	            	   }
	                  }
	                  if(isa<IfStmt>(Else)){
	            	      is_ifelse = true;
	                  }

	              //llvm::errs()<<"ss[15]"<<is_ifelse<<"\n";
	                 if(!is_ifelse){
	                         //write_to_file(Else->getBeginLoc(),"\n//'Else' Single start\n{\n");
	                    write_to_file( recursiveGetStart(Else),"\n//'Else' Single start\n{\n");
	                    write_to_file( recursiveGetEnd(Else),"\n}\n// 'Else' Single end\n");
	                         //write_to_file(Else->getBeginLoc(),"{\n");
	                         //write_to_file( recursiveGetEnd(Else),"\n}\n");
	                 }
	              }
	          }
	      }
          }
	  //llvm::errs()<<"ss[20]\n";
	  if(isa<ForStmt>(currStmt)){
	      if(currStmt->getBeginLoc().isMacroID()){
		  //continue;
	      }else{
                  ForStmt *forStatement = cast<ForStmt>(currStmt);
	          Stmt * forBody =  forStatement->getBody();
	          if(forBody != NULL){
	              if(isa<CompoundStmt>(forBody)){
	              }else{
	                  std::string forstring = GetStmtString(currStmt);	  
                          //llvm::errs()<<"hyhy\n"<<forstring;
                          if(verify_for(forstring)){		  
	            	 //llvm::errs()<<"Verifyed\n";     
	                     write_to_file( recursiveGetStart(forBody),"\n//'For' Single start\n{\n");
	                     write_to_file( recursiveGetEnd(forBody),"\n}\n// 'For' Single end\n");
	                  }else{
	            	 //llvm::errs()<<"false for\n";     
	                  }
	              }
	          }
             }		  
	  }
	  if(isa<WhileStmt>(currStmt)){
	      if(currStmt->getBeginLoc().isMacroID()){
		  //continue;
	      }else{
                   WhileStmt *whileStatement = cast<WhileStmt>(currStmt);
	           Stmt * whileBody =  whileStatement->getBody();
	           if(whileBody != NULL){
	               if(isa<CompoundStmt>(whileBody)){
	               }else{
	                   write_to_file( recursiveGetStart(whileBody),"\n//'While' Single start\n{\n");
	                   write_to_file( recursiveGetEnd(whileBody),"\n}\n// 'While' Single end\n");
	               }
	           }
	      }
	  }
          recursiveVisit(currStmt);
      }
    }
  return;
  }

  bool verify_for(std::string str){

      std::stringstream ss(str);
      std::string line;
      if(std::getline(ss,line,'\n')){
          //llvm::errs()<<"\n"<<line<<"\n";
	  if(line.find("for (") != std::string::npos){
              return true;
	  }
	  if(line.find("for(") != std::string::npos){
	      return true;
	  }
	  if(line.find("_for_") != std::string::npos){
              return true;
	  }
      } 
      return false;
  }	  

  SourceLocation recursiveGetStart(Stmt *stmt){
     SourceLocation startLoc = stmt->getBeginLoc();
     if( startLoc.isMacroID() ) {
         auto expansionRange = 
                  mSM.getExpansionRange( startLoc );
     
         // We're just interested in the start location
         startLoc = expansionRange.getBegin();
     }
     return startLoc;
  
  }
  SourceLocation recursiveGetEnd(Stmt *stmt){
	  
     SourceLocation loc = Lexer::getLocForEndOfToken(stmt->getEndLoc(), 0, mSM, LangOptions());
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
std::string GetStmtString(const Stmt * stmt){
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

 void move_var_decl (Stmt *FuncBody){
	 //var_declVisit visit all the statements in the function recursively
	 //it finds all the decleartion statements which are not in main body of the function
	 //(they are inside an if or a switch case, etc)
	 //it comments that declaration, if there is an assignment at the moment of declaration 
	 //it insert the assignment then it stores all the declaration statements in Infnc_dcls
	 //list as string 
	fncDecls.clear();
	fnc_declVisit(FuncBody);
	//print_fnc_decls();
	varDecls.clear();
	var_declVisit (FuncBody,0);
	//this variable declarations will be inserted at the begining of the function in
	//insert_pairmask_decl_function

 }

void print_fnc_decls(){
    for(itvarDecls=fncDecls.begin() ;itvarDecls!=fncDecls.end();
		    ++itvarDecls){
	
	    varDecl * currDecl = (*itvarDecls);
	    llvm::errs()<<currDecl->str<<" name :"<<currDecl->name<<"\n"
		    <<" type: " <<currDecl->type<<"\n";
    }
}
void fnc_declVisit (Stmt *stmt ) { 
  for (Stmt::child_iterator i = stmt->child_begin(),
			    e = stmt->child_end();
			    i != e;
			    ++i) {
      Stmt *currStmt = (*i);
      if(currStmt==NULL)
	      continue;
      SourceLocation loc = currStmt->getBeginLoc();
      if(!TheRewriter.isRewritable(loc)){
	      continue;
      }
      if (!mSM.isInMainFile(loc)){
	      continue;
      }
      if(isa<DeclStmt>(currStmt)){
	 std::string decl_string;
	 llvm::raw_string_ostream decl_stream(decl_string);
	 currStmt->printPretty(decl_stream, NULL, PrintingPolicy(LangOptions()));
	 decl_stream.flush();
	 DeclStmt * decl_stmt = cast<DeclStmt>(currStmt);
	 std::string declName; 
	 std::string declType;
	 for (DeclStmt::decl_iterator di = decl_stmt->decl_begin(),
				      de = decl_stmt->decl_end();
				      di != de ;
				      ++di){
	     Decl * currDecl = (*di);
	     if(!isa<ValueDecl>(currDecl)){
		 continue;
	     }
	     ValueDecl * namedDecl = cast<ValueDecl>(currDecl);
	     declName = namedDecl->getNameAsString();
	     QualType declType_qt = namedDecl->getType();
	     declType = declType_qt.getAsString();
	     varDecl * myvarDecl = new varDecl();
	     myvarDecl->str = decl_string;
	     myvarDecl->name = declName;
	     myvarDecl->type = declType;
	     myvarDecl->stmt = currStmt;
	     myvarDecl->decl  = currDecl;
	     fncDecls.push_back(myvarDecl);

	 }
      }		
  } 
 
}
 void var_declVisit (Stmt *stmt , int depth ) { 
  depth ++;
  for (Stmt::child_iterator i = stmt->child_begin(),
			    e = stmt->child_end();
			    i != e;
			    ++i) {
      Stmt *currStmt = (*i);
      if(currStmt==NULL)
	      continue;
      SourceLocation loc = currStmt->getBeginLoc();
      SourceLocation locend = currStmt->getEndLoc();
      SourceLocation aftersemiLoc = locend;

      if(!TheRewriter.isRewritable(loc)){
	      continue;
      }
      if (!mSM.isInMainFile(loc)){
	      continue;
      }

      if(isa<DeclStmt>(currStmt) && (depth > 1)){
	 std::string decl_string;
	 llvm::raw_string_ostream decl_stream(decl_string);
	 currStmt->printPretty(decl_stream, NULL, PrintingPolicy(LangOptions()));
	 decl_stream.flush();
	 //llvm::errs()<<GetStmtString(currStmt)<<"\n"; 
	 // to ignore the decls with array initialization
	 if(GetStmtString(currStmt).find("{") != std::string::npos){
	      continue;
	 } 
	 DeclStmt * decl_stmt = cast<DeclStmt>(currStmt);
	 std::string declName; 
	 std::string declType;
	 std::string assignment_string = "";
	 std::string replace_str = "";
	 for (DeclStmt::decl_iterator di = decl_stmt->decl_begin(),
				      de = decl_stmt->decl_end();
				      di != de ;
				      ++di){
	     Decl * currDecl = (*di);
	     if(!isa<ValueDecl>(currDecl)){
                 continue;
	     }
	     ValueDecl * namedDecl = cast<ValueDecl>(currDecl);
	     declName = namedDecl->getNameAsString();
	     QualType declType_qt = namedDecl->getType();
	     declType = declType_qt.getAsString();
	     

	 
	 //currStmt->dumpPretty(mASTcontext); 
	 //llvm::errs()<<"\n";
	 //llvm::errs()<<"name is: " <<declName<<"\n" ;
	 //currStmt->dumpColor();
	    int dupplicate_found = 0;
	    for(itvarDecls = fncDecls.begin();
	           	 itvarDecls != fncDecls.end();
	           	 ++itvarDecls){
	            varDecl * currDecl = (*itvarDecls);
	            if( currDecl->name == declName ){
	           	 dupplicate_found=1;
	           	 break;
	            }
	    }
	    for(itvarDecls = varDecls.begin();
	           	 itvarDecls != varDecls.end();
	           	 ++itvarDecls){
	            varDecl * currDecl = (*itvarDecls);
	            if( currDecl->name == declName ){
	           	 dupplicate_found=1;
	           	 break;
	            }
	    }

	    replace_str = "";
	    std::string declName_orig = "";
	    if (dupplicate_found==1){
	            declName_orig = declName;
	            declName = declName + "_hecaton_" + std::to_string(var_suffix);
	            var_suffix++;
	            replace_str = "//Hecaton_replace " + declName_orig + ":" + declName +"\n";
	    }
	    varDecl * myvarDecl = new varDecl();
	    myvarDecl->str = decl_string;
	    myvarDecl->name = declName;
	    myvarDecl->type = declType;
	    myvarDecl->old_name = declName_orig;
	    myvarDecl->stmt = currStmt;
	    myvarDecl->decl = currDecl;
	    varDecls.push_back(myvarDecl);	
	   
	    VarDecl * variable_decl = cast<VarDecl>(currDecl); 
	    const Expr * myExpr = variable_decl->getAnyInitializer();
	    if( myExpr != NULL){
	         //llvm::errs()<<"[5.4] currStmt\n";     
	         //currStmt->dumpPretty(mASTcontext); 
	         //llvm::errs()<<"[5.5] myExpr\n";     
	         //myExpr->dumpPretty(mASTcontext); 
	         //llvm::errs()<<"\n";
	         std::string stmt_string;
	         llvm::raw_string_ostream stream(stmt_string);
	         // TODO /*Here*/ stmt_string should be replace with unexpanded macro
	         // Javad: Done
	         if( myExpr->getBeginLoc().isMacroID() ){
	             auto unexpanded_range = TheRewriter.getSourceMgr().getExpansionRange(myExpr->getSourceRange());
	             stmt_string = TheRewriter.getRewrittenText(unexpanded_range);
	         }else{
	             myExpr->printPretty(stream, NULL, PrintingPolicy(LangOptions()));
	             stream.flush();
	        }
	        assignment_string += declName + " = " + stmt_string + ";/*Inserted by Hecaton*/\n";
	    
	    }


	 }
	 write_to_file2(loc,assignment_string+ replace_str +"/*Commented by Hecaton*/ /*");
	 write_to_file3(locend," *///");
      }
      var_declVisit (currStmt,depth);
  }
  return;
 }
 
void insert_var_decl( Stmt * Body){
    if (isa<CompoundStmt>(Body)){
        CompoundStmt * cmpnd_stmt = cast<CompoundStmt>(Body);
	SourceLocation ST2 = cmpnd_stmt->getLBracLoc();
	SourceLocation ST = Lexer::getLocForEndOfToken(ST2,0,
		mSM,LangOptions() );	
	if(mSM.isInMainFile(ST)){	
	    if(!varDecls.empty()){
	        write_to_file2(ST, "\n\t/*Hecaton var decl*/\n");
	    }else{
		return;
	    }
	    for ( itvarDecls = varDecls.begin();
	    		itvarDecls != varDecls.end();
	    		++itvarDecls){
	    	varDecl * myvar_decl = (*itvarDecls);
	        //Handle array declaration
	        bool is_array = false;
		auto startpos = myvar_decl->type.find("[");
		std::string array_str = "";
	        if (startpos != std::string::npos){
                   is_array = true;
		   auto endpos = myvar_decl->type.find("]");
		   array_str = myvar_decl->type.substr(startpos);
		   myvar_decl->type = myvar_decl->type.replace(startpos,endpos - startpos +1, "");

	        }
		std::string currstr = "\t" +  myvar_decl->type + " " + myvar_decl->name + 
			 array_str + ";\n";
	        write_to_file2(ST, currstr);

	    }
	    write_to_file2(ST, "\t/*Hecaton var decl End*/\n");
	}
	return;
    }	    
}
void seperate_decl_init( Stmt * Body){
    // to find all the needed seperations
    commentedVars.clear();
    SourceLocation last_decl_loc;	  
    int number_of_seperations = 0;
    for(itvarDecls = fncDecls.begin();
		 itvarDecls != fncDecls.end();
		 ++itvarDecls){
	varDecl * myvar_decl = (*itvarDecls);
	Stmt * mystmt = myvar_decl->stmt;
	if( mystmt == NULL){
	   continue;
	}
	//llvm::errs()<<"fucn decl[0]: "<<GetStmtString(mystmt)<<"\n";
	last_decl_loc = recursiveGetEnd(mystmt);
        Decl * mydecl =  myvar_decl->decl;
	if(mydecl == NULL){
	   continue;
	}
	VarDecl * variable_decl = cast<VarDecl>(mydecl);
	const Expr * myExpr = variable_decl->getAnyInitializer();
	if(myExpr == NULL){
	    continue;
	}
        int is_unique = 1;
        for(itcommentedVars = commentedVars.begin();
                itcommentedVars != commentedVars.end();
                ++itcommentedVars){
             Stmt * currStmt = (*itcommentedVars);
	     if (currStmt->getBeginLoc() == mystmt->getBeginLoc()){
	         is_unique = 0;
	     }
	}
	int has_array_init = 0;
	if ( GetStmtString(mystmt).find("{") != std::string::npos){
	   has_array_init = 1;
	}
        int has_const =0;
	if ( GetStmtString(mystmt).find("const ") != std::string::npos){
	   has_const = 1;
	}

	if((is_unique == 1) && (has_array_init == 0) && (has_const ==0) ){
            		
            commentedVars.push_back(mystmt);
	    number_of_seperations ++;
	}
    }
    //to perform the seperation
    std::string to_insert_str = "\n//Hecaton Init Decl Seperation\n"; 
    std::string declration_str;
    std::string initilazaion_str;
    for(itcommentedVars = commentedVars.begin();
            itcommentedVars != commentedVars.end();
            ++itcommentedVars){
         Stmt * varStmt = (*itcommentedVars);
	 DeclStmt * decl_stmt = cast<DeclStmt>(varStmt);
	 std::string declName; 
	 std::string declType;
	 std::string assignment_string = "";
	 std::string replace_str = "";
	 for (DeclStmt::decl_iterator di = decl_stmt->decl_begin(),
				      de = decl_stmt->decl_end();
				      di != de ;
				      ++di){
	     Decl * currDecl = (*di);
	     if(!isa<ValueDecl>(currDecl)){
                 continue;
	     }
	     ValueDecl * namedDecl = cast<ValueDecl>(currDecl);
	     declName = namedDecl->getNameAsString();
	     QualType declType_qt = namedDecl->getType();
	     declType = declType_qt.getAsString();
	     VarDecl * variable_decl = cast<VarDecl>(currDecl); 
	     auto startpos = declType.find("[");
	     std::string array_str = "";
	     if (startpos != std::string::npos){
	        auto endpos = declType.find("]");
	        array_str = declType.substr(startpos);
	        declType= declType.replace(startpos,endpos - startpos +1, "");

	     }
	     declration_str += declType + " "+ declName +""+ array_str + ";\n";
	     const Expr * myExpr = variable_decl->getAnyInitializer();
	     if( myExpr != NULL){
		     initilazaion_str += declName + " = " + GetStmtString(myExpr) + ";\n";
	     }
	 }
	     
    }	    
    to_insert_str = to_insert_str + declration_str + initilazaion_str + "//Hecaton Init Decl Seperation END\n";
    if(number_of_seperations == 0)
	    to_insert_str = "";
    if(last_decl_loc.isValid())
          write_to_file2( last_decl_loc , to_insert_str);


 //to comment out all the seperated var decls
    for(itcommentedVars = commentedVars.begin();
            itcommentedVars != commentedVars.end();
            ++itcommentedVars){
        Stmt * mystmt = (*itcommentedVars);
	std::string decl_string;
	std::string new_decl_string;
	decl_string = TheRewriter.getRewrittenText(mystmt->getSourceRange());
	if (decl_string.find("/*") == std::string::npos){
 	   new_decl_string = "/*"+ decl_string +"*/"; 
	}else{
	   new_decl_string = decl_string;
	}
	StringRef * new_decl_string_ref = new StringRef(new_decl_string);
	TheRewriter.ReplaceText(mystmt->getSourceRange(),*new_decl_string_ref );
    }

}
void replace_duplicate_vars( Stmt * Body){

    if (isa<CompoundStmt>(Body)){
        CompoundStmt * cmpnd_stmt = cast<CompoundStmt>(Body);
	SourceLocation ST2 = cmpnd_stmt->getLBracLoc();
	SourceLocation ST = Lexer::getLocForEndOfToken(ST2,0,
		mSM,LangOptions() );	
	if(!mSM.isInMainFile(ST)){	
	   return;
	}
    }
    if (!varDecls.empty()){
    } else{
       return;	     
    } 
    for ( itvarDecls = varDecls.begin();
		itvarDecls != varDecls.end();
		++itvarDecls){
	varDecl * myvar_decl = (*itvarDecls);
	if( myvar_decl == NULL){
		continue;
	}
	Stmt * stmt =  myvar_decl->stmt;
	if(stmt == NULL){
	   continue;
	}
	if (myvar_decl->old_name == ""){
	   continue;
	}
	//llvm::errs()<<"trying to find\n";
	//stmt->dumpPretty(mASTcontext);
	Stmt * parent =	find_parent_recursive(stmt , Body);
	//const Stmt * parent =	find_parent(stmt);
        if( parent == NULL){
	   //llvm::errs()<<"error: PARENT IS NOT FOUND\n";
	   continue;
	}else{
	   //llvm::errs()<<"PARENT IS FOUND\n";
	
	    std::string parent_string;
	    std::string new_parent_string;
	    parent_string = TheRewriter.getRewrittenText(parent->getSourceRange());
	    //llvm::errs()<<parent_string;
	    new_parent_string = replace_variable( parent_string , myvar_decl->old_name , myvar_decl->name);
	    
	    //llvm::errs()<<new_parent_string;
	    StringRef * new_parent_string_ref = new StringRef(new_parent_string);
	    TheRewriter.ReplaceText(parent->getSourceRange(),*new_parent_string_ref );
        }

    }

	
    return;
}
std::string replace_variable( std::string text, std::string old_name, std::string new_name){
   std::string new_text;
   std::stringstream ss(text);
   std::string line;
   while(std::getline(ss,line,'\n')){
	//llvm::errs() <<"LINE:\n" <<line <<"\n";
	std::size_t found = line.find("Hecaton");
	if (found!=std::string::npos){
	    //llvm::errs()<<"Hecaton found!! ignore\n";	
	}else{
	   std::size_t found_pos = line.find(old_name);
           while(found_pos!=std::string::npos){
	       //llvm::errs()<<"intersting line\n";
	       bool correct_found = is_valid_variable(line,old_name,found_pos);
	       if(correct_found){
		   line.replace(found_pos,old_name.length(),new_name);    
	           //llvm::errs()<<"correct found\n";
		   //llvm::errs()<<line<<"\n";

	       }else{
	           //llvm::errs()<<"false found\n";
	       }
               found_pos = line.find(old_name,found_pos +1);	       
	   }	   
	}
	new_text = new_text + line + "\n";
   }  
   return new_text;
}
bool is_valid_variable( std::string line , std::string name, std::size_t pos){
   size_t len = name.length();
   char before_ch = line[pos-1];
   char after_ch = line[pos+len];
   //llvm::errs()<<before_ch<<"\t"<<after_ch<<"\n";
   while( (before_ch == ' ') && pos>0){
       pos = pos - 1;
       if(pos == 0){
           before_ch = ';';
	   break;
       }
       before_ch = line[pos -1];
   }
   if (isalnum(before_ch))
      return false;
   if (before_ch == '_')
      return false;
   if (before_ch == '.')
      return false;	   
   if (before_ch == '>'){
       char before_before_ch = line[pos-2];
       if(before_before_ch == '-')
          return false;
   }
   if (isalnum(after_ch))
      return false;
   if (after_ch == '_')
      return false;

   return true;
}
Stmt * find_parent_recursive (Stmt *stmt, Stmt *Body) {
  Stmt * parent = NULL;
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
bool same_stmt(Stmt *stmt1 , Stmt * stmt2){
	if(stmt1->getSourceRange() == stmt2->getSourceRange()){
           return true;
	}else{
           return false;
	}
}
private:
  Rewriter &TheRewriter;
};
//class PrintFunctionsConsumer : public ASTConsumer {
class HecatonConsumer : public ASTConsumer {
  CompilerInstance &Instance;

public:
//  PrintFunctionsConsumer(CompilerInstance &Instance,
//                         std::set<std::string> ParsedTemplates)
//      : Instance(Instance), ParsedTemplates(ParsedTemplates) {}

  HecatonConsumer(CompilerInstance &Instance, Rewriter &R)
      : Instance(Instance), hecaton_visitor(Instance,R) {}
  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i) {
	//(*i)->dump();
    // Traverse the declaration using our AST visitor
	hecaton_visitor.TraverseDecl(*i);
    }

    return true;
  }
//
private:
  HecatonASTVisitor hecaton_visitor;	

};

//class PrintFunctionNamesAction : public PluginASTAction {
//  std::set<std::string> ParsedTemplates;
class HecatonAction : public PluginASTAction {
public:
   std::string generate_file_name(std::string path){
	   path = "XXXPATHXXX" + path;
    	   llvm::errs() << path<<"\n";
           return path;
   }

  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    llvm::errs() << "\n\n\n** Hecaton Pass1 **\n\n\n ";
    llvm::errs() << "** EndSourceFileAction for: "
                 << SM.getFileEntryForID(SM.getMainFileID())->getName().str() << "\n";
    std::string main_file_str = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
    std::string new_file_str = generate_file_name(main_file_str);
    std::error_code error_code;
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
    ros << "Help of the hecaton-pass1 plugin\n";
  }
private:
  Rewriter TheRewriter;
};

}

static FrontendPluginRegistry::Add<HecatonAction>
X("hecaton-pass1", "print hecaton function");
