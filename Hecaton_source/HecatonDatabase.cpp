//===- HecatonDatabase.cpp ---------------------------------------------===//
//
// This is the Hecaton's helper plug-in for database generation
// 
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
using namespace clang;

namespace {
//data structures I am using to keep track of error handling codes
//Hecaton: a data struct to keep the first statement of EHC in the first round of analysis
enum EHC_type {GOTO,RETURN,COND};
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
class Function{
	public:
		Stmt * Body;
		std::list<EHC *> EHCs;
		std::list<EHC *>::iterator itEHCs;
		std::string typeStr;
		std::string name;

};
std::list<Function*> listOfFunctions;
std::list<Function*>::iterator itFunctions;
//----------------------------------------------------------//


// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class HecatonASTVisitor : public RecursiveASTVisitor<HecatonASTVisitor> {
  CompilerInstance &Instance;
  SourceManager &mSM = Instance.getSourceManager();
  ASTContext &mASTcontext = Instance.getASTContext();
public:
  HecatonASTVisitor(CompilerInstance &Instance)
      : Instance(Instance) {}
  bool VisitStmt(Stmt *s) {
    return true;
  }

//Hecaton: to visit all of the functions in the code
  bool VisitFunctionDecl(FunctionDecl *f) {
    // Only function definitions (with bodies), not declarations.
    if (f->hasBody()) {
      if(!mSM.isInMainFile(f->getBeginLoc())){
              return true;
      }
//Hecaton: find basic information of the function and make a Function data struct      
      Stmt *FuncBody = f->getBody();



      // Type name as string
      QualType QT = f->getReturnType();
      std::string TypeStr = QT.getAsString();

      // Function name
      DeclarationName DeclName = f->getNameInfo().getName();
      std::string FuncName = DeclName.getAsString();
      llvm::errs()<<FuncName<<"\n";

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

//Hecaton: to find all of the if condintions inside the every functions bodies
//and find whether the if or else body is EHC
//recursiveVisit_if stores the begining node of each EHC in EHC_heads list
      EHC_heads.clear();
      recursiveVisit_if(FuncBody);
//Hecaton to eleminate duplicates in EHC_heads      
      ehc_head_clean_duplicates();
//Hecaton: to find every statements in EHCs based on the EHC_heads list
//we need to resolve the goto statements
      find_ehc_stmts(myfunction);

      delete_printks(myfunction);
      handle_return(myfunction);
      find_ehc_new_stmts(myfunction);
      find_pairs(myfunction);

      write_pairs_to_database(myfunction);

      listOfFunctions.push_back(myfunction);

      return true;
   }
   return true;
 }

 void recursiveVisit_if (Stmt *stmt) {
  	  for (Stmt::child_iterator i = stmt->child_begin(),
			            e = stmt->child_end();
				    i != e;
				    ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if(isa<IfStmt>(currStmt)){
			//here we have found all the if statements
		         IfStmt *IfStatement = cast<IfStmt>(currStmt);
		         Stmt *Then = IfStatement->getThen();
		         Expr *condition = IfStatement->getCond();
			 if(condition == NULL)
			 	continue;	 
		         if(isa<BinaryOperator>(condition)){
		           	BinaryOperator *binop = cast<BinaryOperator>(condition);
		           	if(binop->isComparisonOp ()){	
		           		// has a comparison oprator
		           		Expr *right_hand = binop->getRHS();
					if(right_hand!=NULL)
		           		if(isa<IntegerLiteral>(right_hand)){
		           			IntegerLiteral* int_right_hand = cast<IntegerLiteral>(right_hand);
		           			int value = int_right_hand->getValue().getLimitedValue();
		           			if ((binop->getOpcodeStr().str() == "<") && (value==0)){
		           			//this is an error handling code becasue of <0 comparision in the condintion
							EHC_head * ehc_head = new EHC_head();
							ehc_head->type = COND;
							ehc_head->headStmt = Then;
							EHC_heads.push_back(ehc_head);
		           			}

		           		}
		           	}
		         }

      			 if( isa<CallExpr>(condition) ){
      			         CallExpr * callexp = cast<CallExpr>(condition);
      			         FunctionDecl * func_decl = callexp->getDirectCallee();
      			         if(func_decl !=0){
      			   	      std::string func_name = func_decl->getNameInfo().getAsString();
      			   	      if( (func_name == "IS_ERR") || func_name == "IS_ERR_OR_NULL"){
						//this is an error handling code becasue IS_ERR or IS_ERR_OR_NULL in teh condintion
							EHC_head * ehc_head = new EHC_head();
							ehc_head->type = COND;
							ehc_head->headStmt = Then;
							EHC_heads.push_back(ehc_head);
      			   	      }

      			         }
      			 }
			 int return_neg_value;
			 return_neg_value = Visit_return(Then);
			 if ( return_neg_value == 1){
				// This is an EHC becuase of negative return value in the Then part
				EHC_head * ehc_head = new EHC_head();
				ehc_head->type = RETURN;
				ehc_head->headStmt = Then;
				EHC_heads.push_back(ehc_head);
			 }
			 int has_goto;
      			 has_goto = Visit_goto(Then);
			 if ( has_goto == 1){
				//This is an EHC becuase of a goto in Then part 
				EHC_head * ehc_head = new EHC_head();
				ehc_head->type = GOTO;
				ehc_head->headStmt = Then;
				EHC_heads.push_back(ehc_head);
			 }

		         Stmt *Else = IfStatement->getElse();
		         if (Else){
			   int return_neg_value;
			   return_neg_value = Visit_return(Else);
			   if ( return_neg_value == 1){
			          //This is an EHC becuase of negative return value in Else part of the code
			          EHC_head * ehc_head = new EHC_head();
			          ehc_head->type = RETURN;
			          ehc_head->headStmt = Else;
			          EHC_heads.push_back(ehc_head);
			   }
			   int has_goto;
      			   has_goto = Visit_goto(Else);
			   if ( has_goto == 1){
			          //This is an EHC becuase of a goto in ELSE part of the code
			          EHC_head * ehc_head = new EHC_head();
			          ehc_head->type = GOTO;
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
//Hecaton to check if the return value in the block is negative or not. 
int  Visit_return (Stmt *stmt) {
	  int ret = 0 ;
  	  for (Stmt::child_iterator i = stmt->child_begin(), e = stmt->child_end(); i != e; ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if(isa<ReturnStmt>(currStmt)){
	   		ReturnStmt * RetStatement = cast<ReturnStmt>(currStmt);
			//RetStatement->dumpPretty(*mASTcontext);
			//RetStatement->dumpColor();
	   		Expr *retval = RetStatement->getRetValue();
	   		if(retval!=NULL){
				if(isa<ImplicitCastExpr>(retval)){
					ImplicitCastExpr * ImCaEx = cast<ImplicitCastExpr>(retval);
					retval = ImCaEx -> getSubExprAsWritten();
				}
	   		        if(isa<UnaryOperator>(retval)){
	   		     	   std::string uniop_str="";   
	   		     	   UnaryOperator* uniop = cast<UnaryOperator>(retval);
	   		     	   if (uniop->getOpcode() == UO_Minus){

	   		     		uniop_str = " - ";
					ret = 1;
      	   				//has UnaryOperator "-"
					break;
	   		     	   }
      	   			   //has UnaryOperator other than "-"
	   		        }
	   		}
		    }
	      }
	   } 
	   return ret;
}
//To check whether ther is a goto statement in the block
 int Visit_goto (Stmt *stmt) {
	  int ret = 0;
	  if(stmt!=NULL){
	       if(isa<GotoStmt>(stmt)){
		       return 1;
	       }
	  }
  	  for (Stmt::child_iterator i = stmt->child_begin(), e = stmt->child_end(); i != e; ++i) {
  	      Stmt *currStmt = (*i);
	      if(currStmt!=NULL){
		    if(isa<GotoStmt>(currStmt)){
			   ret=1;
			   break;
		    }
	      }
	   } 
	   return ret;
}
  void ehc_head_clean_duplicates(){
      for (itEHC_heads = EHC_heads.begin() ;
		      itEHC_heads != EHC_heads.end();
		      ++itEHC_heads){
	      EHC_head * ehc = (*itEHC_heads);
	      //Hecaton: An EHC might be found twice with COND and with RETURN/GOTO, remove the one with 
	      //COND
	      if(ehc->type != COND){
      	         for (it2EHC_heads = EHC_heads.begin() ;
				 it2EHC_heads != EHC_heads.end();
				 ){
	      	      EHC_head * ehc2 = (*it2EHC_heads);
		      if((ehc2->type==COND)&&(ehc2->headStmt==ehc->headStmt)){
			it2EHC_heads = EHC_heads.erase(it2EHC_heads);
		      }else{
		      	++it2EHC_heads;
		      }

		 }
	      }
      }
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
	      if(my_ehc_head->type == GOTO){
		      GotoStmt * goto_stmt;
		      LabelDecl * labeldec;
		      LabelStmt * labelstm;
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
		      for (Stmt::child_iterator i = myfunction->Body->child_begin(),
				      e = myfunction->Body->child_end();
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
					  myehc->ehc_stmts.push_back(newStmt);
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
		      for (Stmt::child_iterator i = my_ehc_head->headStmt->child_begin(),
				      e = my_ehc_head->headStmt->child_end();
				      i != e;
				      ++i) {
			      Stmt *currStmt = (*i);
			      myehc->ehc_stmts.push_back(currStmt);
		      }
	      }
	      myfunction->EHCs.push_back(myehc);
      }
}
void delete_printks(Function * myfunction){
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
        	      if (isa<CallExpr>(currStmt)){
        		      CallExpr * callexpr = cast<CallExpr>(currStmt);
        		      FunctionDecl * fncdcl = callexpr->getDirectCallee();
			      if(fncdcl!=NULL){
        		          DeclarationName declname = fncdcl->getNameInfo().getName();
        		          std::string fncname = declname.getAsString();
        		          if (fncname=="printk"){
        		              delete_this_stmt = true;

        		          }
			      }
        	      }
		      std::string stmt_string;
		      llvm::raw_string_ostream stream(stmt_string);

		      currStmt->printPretty(stream, NULL, PrintingPolicy(LangOptions()));
		      stream.flush();
		      if( (stmt_string.find("pr_debug") != std::string::npos) ){
		      	delete_this_stmt = true;
		      }

		      if( (stmt_string.find("break") != std::string::npos) ){
		      	delete_this_stmt = true;
		      }

		      if( (stmt_string.find("rc =") != std::string::npos) ){
		      	delete_this_stmt = true;
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
void  find_ehc_new_stmts(Function * myfunction){

      std::list<EHC *>::iterator itEHCs2;
      std::list<Stmt*>::iterator it_ehc_stmts2;
      bool is_new_statement = false;
      for (myfunction->itEHCs = myfunction->EHCs.begin();
		      myfunction->itEHCs != myfunction->EHCs.end();
		      ++myfunction->itEHCs){

	  EHC * ehc = (*myfunction->itEHCs);
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
		      stream.flush();
		      stream2.flush();

		      if(stmt_string2 == stmt_string){
			      is_new_statement = false;
			      break;
		      }


	          }
      	      }
	      if(is_new_statement){
		      EHC_new_stmt * ehc_new_stmt = new EHC_new_stmt();
		      ehc_new_stmt->stmt = currStmt ;
		      ehc->ehc_new_stmts.push_back(ehc_new_stmt);
		      
	      } 
	  }
      }

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
	    
      ///*tt*/llvm::errs()<<"tt[1]\n";
        
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
      ///*tt*/llvm::errs()<<"tt[2]\n";

		
        
        EHC * ehc = (*myfunction->itEHCs);
	end_stmt = ehc->head->headStmt;
	end_loc = end_stmt->getBeginLoc();
        end_line = mSM.getExpansionLineNumber(end_loc);


//	llvm::errs()<<"start line = "<<start_line<<"\tend line = "<<end_line<<"\n\n\n";
      ///*tt*/llvm::errs()<<"tt[3]\n";
        for( ehc->it_ehc_new_stmts  = ehc->ehc_new_stmts.begin();
	     ehc->it_ehc_new_stmts != ehc->ehc_new_stmts.end();
	     ++ehc->it_ehc_new_stmts ){
      	///*tt*/llvm::errs()<<"tt[3.1]\n";
	    EHC_new_stmt *  ehcStmt = (*(ehc->it_ehc_new_stmts));
      	///*tt*/llvm::errs()<<"tt[3.2]\n";
	    int pair_found = 0;	
            pair_found = recursive_pair_visit(myfunction->Body,ehcStmt,start_line,end_line);
      	///*tt*/llvm::errs()<<"tt[3.3]\n";
	    ehcStmt->is_outer = 0;
	    if( pair_found){
		    ehcStmt->is_outer = 1;
		    //llvm::errs()<<"pair found[2]\n";
	    }else{
      		///*tt*/llvm::errs()<<"tt[4]\n";
    		    possible_pairs.clear();
                    recursive_probable_pair_visit(myfunction->Body,ehcStmt,start_line,end_line);
      		///*tt*/llvm::errs()<<"tt[5]\n";
		    sorted_pairs.clear();
		    sort_pairs();
      		///*tt*/llvm::errs()<<"tt[6]\n";
		    pick_the_most_probable(ehcStmt);
      		///*tt*/llvm::errs()<<"tt[7]\n";
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
	            	          //llvm::errs()<<"pair found\n";
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
				   loc = mSM.getExpansionRange(loc).getEnd();	
				}
				loc.dump(mSM);	
	                        //llvm::errs()<<"\npair found\n";
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
void recursive_probable_pair_visit(Stmt * stmt, EHC_new_stmt * ehcStmt, int start_line, int end_line){
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	if( currStmt == NULL)
		continue;
	bool pair_candidate=false;

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
      ///*tt*/llvm::errs()<<"jj[1]\n";
                possible_pair->probability = pair_evaluation(ehcStmt->stmt,currStmt);
      ///*tt*/llvm::errs()<<"jj[2]\n";
                possible_pairs.push_back(possible_pair);
        }

        recursive_probable_pair_visit(currStmt, ehcStmt, start_line,end_line);
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
    int len2 = stmt_string2.length();
    if( len >200 || len2>200){
	return 0;
     }
     // /*tt*/llvm::errs()<<"ww[1]\n"<<stmt_string1<<","<<stmt_string2<<"\n";
    score = compare (&stmt_string1,&stmt_string2, 0); 
     // /*tt*/llvm::errs()<<"ww[2]\n";

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
void  write_pairs_to_database(Function * currFunction){
      std::fstream fs;
      fs.open ("XXXPATHXXX", std::fstream::out | std::fstream::app);
      for ( currFunction->itEHCs = currFunction->EHCs.begin();
          	    currFunction->itEHCs != currFunction->EHCs.end();
          	    ++(currFunction->itEHCs)){
          EHC * currEHC = (*(currFunction->itEHCs));
          for( currEHC->it_ehc_new_stmts = currEHC->ehc_new_stmts.begin();
          	    currEHC->it_ehc_new_stmts != currEHC->ehc_new_stmts.end();
          	    ++(currEHC->it_ehc_new_stmts)){
              EHC_new_stmt * curr_ehc_new_stmt = (*(currEHC->it_ehc_new_stmts));
              Stmt * stmt = curr_ehc_new_stmt->stmt ;
              Stmt * pair = curr_ehc_new_stmt->pair;

              std::string stmt_string;
              llvm::raw_string_ostream stream(stmt_string);
              stmt->printPretty(stream, NULL, PrintingPolicy(LangOptions()));
              stream.flush();
              if( (stmt_string.find("print") != std::string::npos) ){
          	continue;
              }
              std::string name = find_fnc_name(stmt);
              if(name != ""){
          	fs<<name<<",";
                std::string pair_name = find_fnc_name(pair);
          	fs<<pair_name<<"\n";
              }

          }
      }
      fs.close();
}
std::string find_fnc_name(Stmt * stmt){

    if(stmt!=NULL){
       if (isa<CallExpr>(stmt)){
    	 //llvm::errs()<<"call expression found\n";	
    	 CallExpr * callexpr = cast<CallExpr>(stmt);
    	 FunctionDecl * fncdcl = callexpr->getDirectCallee();
    	 if(fncdcl != NULL){
    	     //llvm::errs()<<"fncdcl found\n";	
    	     DeclarationName declname = fncdcl->getNameInfo().getName();
    	     std::string fncname = declname.getAsString();
    	     //llvm::errs()<<fncname<<"\n";
	     return fncname; 
    	 }
      }
    }
    return recursive_fnc_name(stmt);
}
std::string recursive_fnc_name(Stmt * stmt){
    std::string found = "";
    if (stmt == NULL){
	return found;
    }	
    for (Stmt::child_iterator i = stmt->child_begin(),
          		    e = stmt->child_end();
          		    i != e;
          		    ++i) {
        Stmt *currStmt = (*i);
	found = "";
        if(currStmt!=NULL){
           if (isa<CallExpr>(currStmt)){
		 //llvm::errs()<<"call expression found\n";	
                 CallExpr * callexpr = cast<CallExpr>(currStmt);
                 FunctionDecl * fncdcl = callexpr->getDirectCallee();
                 if(fncdcl != NULL){
		     //llvm::errs()<<"fncdcl found\n";	
                     DeclarationName declname = fncdcl->getNameInfo().getName();
                     std::string fncname = declname.getAsString();
		     //llvm::errs()<<fncname<<"\n";
		     found = fncname; 
		     return found;
		 }
          } 
	    found = recursive_fnc_name(currStmt);
        }
    }
    return found;
}
};
class HecatonConsumer : public ASTConsumer {
  CompilerInstance &Instance;

public:
  HecatonConsumer(CompilerInstance &Instance)
      : Instance(Instance), hecaton_visitor(Instance) {}
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
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<HecatonConsumer>(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    if (!args.empty() && args[0] == "help")
      PrintHelp(llvm::errs());

    return true;
  }
  void PrintHelp(llvm::raw_ostream& ros) {
    ros << "Help for Hecaton_database plugin goes here\n";
  }

};

}

static FrontendPluginRegistry::Add<HecatonAction>
X("hecaton-plugin", "print hecaton pairs");
