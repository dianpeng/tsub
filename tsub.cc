#include "tsub.h"
#include <cerrno>
#include <cstdlib>
#include <cstdarg>
#include <sstream>
#include <cstdio>
#include <set>

#define UNREACHABLE(X) do { assert(0&&"Unreachable"); X; } while(0)

namespace {

// String as array function. Ugly hack to get the internal buffer of that
// std::string object. This one _may_ portable , this is because Chrome uses
// a similar trick to do so in its base/stl_helper.h file (may not correct).

const char* StringAsArray( const std::string& str , int pos ) {
    return &(*(str.begin() + pos));
}

namespace exp {

using tsub::Value;
using tsub::ValueList;
using tsub::Context;

enum TokenId {
    TK_ADD,TK_SUB,TK_MUL,TK_DIV,
    TK_LT,TK_LET,TK_GT,TK_GET,TK_EQ,TK_NEQ,
    TK_AND,TK_OR,TK_NOT,TK_QUESTION,TK_COLON,
    TK_STRING,TK_VARIABLE,TK_NUMBER,
    TK_LPAR,TK_RPAR,TK_COMMA,TK_DOLLAR,
    TK_LSQR,TK_RSQR,TK_LBRA,TK_RBRA,TK_TO,
    TK_EOF,TK_UNKNOWN
};

const char* GetTokenName( TokenId tk ) {
    switch(tk) {
#define _DO(tk,n) case tk: return n
        _DO(TK_ADD,"+");
        _DO(TK_SUB,"-");
        _DO(TK_MUL,"*");
        _DO(TK_DIV,"/");
        _DO(TK_LT,"<");
        _DO(TK_LET,"<=");
        _DO(TK_GT,">");
        _DO(TK_GET,">=");
        _DO(TK_EQ,"==");
        _DO(TK_NEQ,"!=");
        _DO(TK_AND,"&&");
        _DO(TK_OR,"||");
        _DO(TK_NOT,"!");
        _DO(TK_STRING,"<string>");
        _DO(TK_VARIABLE,"<variable>");
        _DO(TK_NUMBER,"<number>");
        _DO(TK_LPAR,"(");
        _DO(TK_RPAR,")");
        _DO(TK_LSQR,"[");
        _DO(TK_RSQR,"]");
        _DO(TK_LBRA,"{");
        _DO(TK_RBRA,"}");
        _DO(TK_TO,"..");
        _DO(TK_COMMA,",");
        _DO(TK_QUESTION,"?");
        _DO(TK_COLON,":");
        _DO(TK_DOLLAR,"$");
        _DO(TK_EOF,"<eof>");
        _DO(TK_UNKNOWN,"<unknown>");
#undef _DO
        default:
            return "<unknown>";
    }
}

struct Lexme {
    TokenId token;
    std::size_t offset;

    Lexme( TokenId tk , std::size_t off ):
        token(tk),
        offset(off)
        {}

    Lexme():
        token(TK_UNKNOWN),
        offset(0)
        {}
};

static bool IsIdInitialChar( int ch ) {
    return ch == '_' || std::isalpha(ch) ;
}

static bool IsIdRestChar( int ch ) {
    return IsIdInitialChar(ch) || std::isdigit(ch);
}

class Scanner {
public:
    Scanner( const std::string& source , int pos ) :
        position_(pos),
        start_position_(pos),
        source_(&source) {
            Next();
        }

    Lexme Next() {
        return (lexme_ = Peek());
    }

    Lexme Peek() const;
    Lexme lexme() const {
        return lexme_;
    }

    int position() const {
        return position_;
    }

    Lexme Move() {
        assert(lexme_.offset != 0);
        position_ += static_cast<int>( lexme_.offset );
        return Next();
    }
    Lexme Move( std::size_t offset ) {
        position_ += static_cast<int>( offset );
        return Next();
    }

    Lexme Set( int offset ) {
        position_ = offset;
        return Next();
    }

    void GetLocation( int* line , int* pos );
private:
    void SkipSpace() const;

    int NChar( int pos ) const {
        return static_cast<std::size_t>(pos) < source_->size() ? source_->at(pos) : 0 ;
    }

private:
    Lexme lexme_;
    mutable int position_;
    int start_position_;
    const std::string* source_;
};

void Scanner::SkipSpace() const {
    while( position_ < static_cast<int>(source_->size()) ) {
        if ( !std::isspace(source_->at(position_)) )
            break;
        position_++;
    }
}


void Scanner::GetLocation( int* line , int* pos )  {
    *line = *pos = 1;
    for( int i = start_position_ ; i < position_ ; ++i ) {
        if( source_->at(i) == '\n' ) {
            *pos = 1;
            ++(*line);
        } else {
            ++(*pos);
        }
    }
}

Lexme Scanner::Peek() const {
    do {
        int cha = NChar(position_);
        switch(cha) {
            case 0:
                return Lexme(TK_EOF,0);
            case '\r':case '\n':case '\t':
            case ' ' :case '\v':
                SkipSpace();
                continue;
            case '+':
                return Lexme(TK_ADD,1);
            case '-':
                return Lexme(TK_SUB,1);
            case '*':
                return Lexme(TK_MUL,1);
            case '$':
                return Lexme(TK_DOLLAR,1);
            case '/':
                return Lexme(TK_DIV,1);
            case '>':
                if( NChar(position_+1) == '=' )
                    return Lexme(TK_GET,2);
                else
                    return Lexme(TK_GT,1);
            case '<':
                if( NChar(position_+1) == '=' )
                    return Lexme(TK_LET,2);
                else
                    return Lexme(TK_LT,1);
            case '=':
                if( NChar(position_+1) == '=' )
                    return Lexme(TK_EQ,2);
                else
                    return Lexme();
            case '!':
                if( NChar(position_+1) == '=' )
                    return Lexme(TK_NEQ,2);
                else
                    return Lexme(TK_NOT,1);
            case '&':
                if( NChar(position_+1) == '&' )
                    return Lexme(TK_AND,2);
                else
                    return Lexme();
            case '|':
                if( NChar(position_+1) == '|' )
                    return Lexme(TK_OR,2);
                else
                    return Lexme();
            case '?':
                return Lexme(TK_QUESTION,1);
            case ':':
                return Lexme(TK_COLON,1);
            case ',':
                return Lexme(TK_COMMA,1);
            case '(':
                return Lexme(TK_LPAR,1);
            case ')':
                return Lexme(TK_RPAR,1);
            case '[':
                return Lexme(TK_LSQR,1);
            case ']':
                return Lexme(TK_RSQR,1);
            case '{':
                return Lexme(TK_LBRA,1);
            case '}':
                return Lexme(TK_RBRA,1);
            case '.':
                if( NChar(position_+1) == '.' )
                    return Lexme(TK_TO,2);
                else
                    return Lexme();
            case '\"':
                return Lexme(TK_STRING,0);
            case '0':case '1':case '2':case '3':case '4':
            case '5':case '6':case '7':case '8':case '9':
                return Lexme(TK_NUMBER,0);
            default:
                if( IsIdInitialChar( NChar(position_) ) )
                    return Lexme(TK_VARIABLE,0);
                else
                    return Lexme();
        }
    } while(true);
}

class Interp {
public:
    Interp( const std::string& source,
            int pos,
            Context* context,
            std::string* error ):

        source_(&source),
        scanner_(source,pos),
        context_(context),
        dollar_value_(NULL),
        error_(error){}

    bool DoInterp( Value* val , int* cur_pos ) {
        if(!InterpExp(val))
            return false;
        else {
            *cur_pos = scanner_.position();
            return true;
        }
    }

private:
    void ReportError( const char* format , ... );
    bool IsEscapeChar( int cha ) {
        switch(cha) {
            case 'n':
            case 't':
            case 'r':
            case 'b':
            case '\"':
            case '\\':
                return true;
            default:
                return false;
        }
    }


    bool InterpListRange( Value* to );
    bool InterpList  ( Value* output );
    bool InterpFunc  ( const std::string& func_name , Value* output );
    bool InterpPF    ( Value* output );
    bool InterpAtomic( Value* output );
    bool InterpUnary ( Value* output );
    bool InterpFactor( Value* output );
    bool InterpTerm  ( Value* output );
    bool InterpComp  ( Value* output );
    bool InterpLogic ( Value* output );
    bool InterpPostExp( Value* output );
    bool InterpTenery( Value* output );
    bool InterpExp   ( Value* output );

    bool ToBool( const Value& cond );

private:

    bool ParseNumber( Value* output );
    bool ParseString( Value* output );
    bool ParseVariable( std::string* var );


private:
    const std::string* source_;
    Scanner scanner_;
    Context* context_;
    const Value* dollar_value_;
    std::string* error_;
};

bool Interp::ToBool( const Value& cond ) {
    switch(cond.type()) {
        case Value::VALUE_STRING:
        case Value::VALUE_LIST:
            return true;
        case Value::VALUE_NUMBER:
            return cond.GetNumber() != 0;
        case Value::VALUE_NULL:
            return false;
        default:
            UNREACHABLE(return false);
    }
}

void Interp::ReportError( const char* format , ... )  {
    char msg[1024];
    va_list vlist;
    std::stringstream formatter;
    int line, pos;

    va_start(vlist,format);
    vsprintf(msg,format,vlist);

    scanner_.GetLocation(&line,&pos);
    formatter<<"[Module:Interp,Location:("<<
        line<<","<<pos<<")]:\n"<<msg<<"\n";
    *error_ = formatter.str();
}



bool Interp::ParseNumber( Value* output ) {
    assert( scanner_.lexme().token == TK_NUMBER );
    // Parsing the number from the current stream, we just use strtol
    errno = 0;
    char* pend;

    long val = std::strtol(
        StringAsArray( *source_ , scanner_.position() ),
        &pend , 10 );

    if( errno ) {
        return false;
    } else {
        scanner_.Move( pend - StringAsArray(*source_, scanner_.position() ) );
        output->SetNumber( static_cast<int>(val) );
        return true;
    }

}

bool Interp::ParseVariable( std::string* variable ) {
    assert( scanner_.lexme().token == TK_VARIABLE );
    int i;

    for( i = static_cast<int>(scanner_.position())+1 ;
             i < static_cast<int>(source_->size()) && IsIdRestChar( source_->at(i) ) ;
             ++i ) ;

    variable->assign(
        StringAsArray(*source_,scanner_.position() ),
        i-scanner_.position() );
    scanner_.Set( i );
    return true;
}

bool Interp::ParseString( Value* output ) {
    assert( scanner_.lexme().token == TK_STRING );
    assert( source_->at( scanner_.position() ) == '\"' );

    int i;
    std::string buffer;

    for( i = static_cast<int>(scanner_.position()) + 1 ;
         i < static_cast<int>(source_->size()) ; ++i ) {
        if( source_->at(i) == '\\' ) {
            // Checking if we are meeting some sort of escape character here
            if( i+1 < static_cast<int>(source_->size()) ) {
                if( IsEscapeChar( source_->at(i+1) ) ) {
                    ++i;
                    buffer.push_back( source_->at(i) );
                    continue;
                }
            }
        }

        if( source_->at(i) == '\"' )
            break;
        buffer.push_back( source_->at(i) );

    }

    if( i == static_cast<int>(source_->size()) ) {
        return false;
    } else {
        assert( source_->at(i) == '\"' );
        scanner_.Set(i+1);
        output->SetString(buffer);
        return true;
    }
}

bool Interp::InterpAtomic( Value* output ) {
    switch( scanner_.lexme().token ) {
        case TK_LSQR:
            // [ means a list literal is appeared, just parse it as a atomic value
            return InterpList( output );
        case TK_DOLLAR:
            if( dollar_value_ == NULL ) {
                ReportError("Dollar value is not set!");
                return false;
            } else {
                *output = *dollar_value_;
                scanner_.Move();
                return true;
            }
        case TK_VARIABLE:
            return InterpPF(output);
        case TK_NUMBER:
            return ParseNumber(output);
        case TK_STRING:
            return ParseString(output);
        case TK_LPAR:
            scanner_.Move();
            if(!InterpExp(output))

                return false;
            if( scanner_.lexme().token != TK_RPAR ) {
                ReportError("Expect ')'");
                return false;
            } else {
                scanner_.Move();
                return true;
            }
        default:
            ReportError("Unexpected token:%s",GetTokenName(scanner_.lexme().token));
            return false;
    }
}

bool Interp::InterpListRange( Value* output ) {
    if( scanner_.lexme().token != TK_TO )
        return false;
    else {
        scanner_.Move();
        return InterpExp(output);
    }
}

bool Interp::InterpList( Value* output ) {
    // List literal has grammer like this : [ exp , exp , exp..exp ]
    // Just to make sure that we understand the \"..\" which means to
    // literal. And we need to expand the to literal once we meet it
    assert( scanner_.lexme().token == TK_LSQR );
    scanner_.Move();

    // Checking the empty list here, we don't allow empty list since
    // it doesn't make any sense here , also empty list makes post
    // expression hard to implement

    if( scanner_.lexme().token == TK_RSQR ) {
        // This is an empty list, just report it as an error
        ReportError("List should not be empty!");
        return false;
    }

    // We don't have unique_ptr in C++03, just use raw pointer but make
    // sure we delete it in any exit statements
    ValueList* vl = new ValueList();

    do {
        Value val;
        Value to; // Optional we have a to

        // Parsing it as an expression here
        if( !Interp::InterpExp(&val) ) {
            delete vl;
            return false;
        }

        // Checking if we meet range statements here
        if( InterpListRange(&to) ) {
            // We got a to, just expand this range to actual list
            if( to.type() != Value::VALUE_NUMBER ||
                val.type()!= Value::VALUE_NUMBER ) {
                delete vl;
                // For simplicity , we currently only allows the type number
                // to have to operator .
                ReportError("\"..\" operator can have operand number");
                return false;
            } else {
                // Now expanding the fr and to range
                int fr = val.GetNumber();
                int en = to.GetNumber();
                if( fr >= en ) {
                    delete vl;
                    ReportError("\"..\" operator must have a strictly less than relation for its left and right operands");
                    return false;
                }
                // Expanding the range to the value list elements
                for( ; fr < en ; ++fr ) {
                    vl->AddValue(fr);
                }
            }
        } else {
            vl->AddValue(val);
        }

        // Now we check that we can meet the , or ] here
        if( scanner_.lexme().token == TK_COMMA ) {
            scanner_.Move();
            continue;
        } else if( scanner_.lexme().token == TK_RSQR ) {
            scanner_.Move();
            break;
        } else {
            delete vl;
            ReportError("list literal has unexpected token:%s",GetTokenName( scanner_.lexme().token ) );
            return false;
        }

    } while(true);

    output->SetList( vl );
    return true;
}

bool Interp::InterpFunc( const std::string& func_name , Value* output ) {
    assert( scanner_.lexme().token == TK_LPAR );
    scanner_.Move();

    std::vector<Value> par ;

    do {
        Value val;
        if( !InterpExp(&val) )
            return false;
        par.push_back(val);
        // Checking the comma or the RPAR
        if( scanner_.lexme().token == TK_COMMA ) {
            scanner_.Move();
            continue;
        } else if( scanner_.lexme().token == TK_RPAR ) {
            scanner_.Move();
            break;
        } else {
            // Unknown token here
            ReportError("Unexpected token:%s",GetTokenName(scanner_.lexme().token));
            return false;
        }
    } while(true);

    if( context_ == NULL ) {
        ReportError("Function:%s doesn't have context to be executed",
            func_name.c_str());
        return false;
    } else {
        std::string error;
        if( !context_->ExecFunction(func_name,par,output,&error) ) {
            ReportError("Function:%s cannot be executed with error:%s",
                func_name.c_str(),
                error.c_str());
            return false;
        } else {
            return true;
        }
    }
}


bool Interp::InterpPF( Value* output ) {
    // Variable prefix expression, could be variable reference or function call
    assert( scanner_.lexme().token == TK_VARIABLE );
    std::string var;
    if( !ParseVariable(&var) ) {
        return false;
    }

    if( scanner_.lexme().token == TK_LPAR ) {
        // A function call goes here
        return InterpFunc(var,output);
    } else {
        if( context_ == NULL ) {
            ReportError("Variable:%s doesn't have context to look up",var.c_str());
            return false;
        } else {
            if( !context_->GetVariable(var,output) ) {
                ReportError("Variable:%s is not existed",var.c_str());
                return false;
            }
            return true;
        }
    }
}

bool Interp::InterpUnary( Value* output ) {
    switch( scanner_.lexme().token ) {
        case TK_ADD:
            scanner_.Move();
            if(!InterpAtomic(output))
                return false;
            if( output->type() != Value::VALUE_NUMBER ) {
                ReportError("Cannot prefix +/- for string");
                return false;
            }
            return true;
        case TK_SUB:
            scanner_.Move();
            if(!InterpAtomic(output))
                return false;
            if( output->type() != Value::VALUE_NUMBER ) {
                ReportError("Cannot prefix +/- for string");
                return false;
            } else {
                output->SetNumber( -output->GetNumber() );
                return true;
            }
         case TK_NOT:
            scanner_.Move();
            if(!InterpAtomic(output))
                return false;
            switch( output->type() ) {
                case Value::VALUE_NUMBER:
                    output->SetNumber(!output->GetNumber());
                    return true;
                case Value::VALUE_STRING:
                    output->SetNumber(0);
                    return true;
                case Value::VALUE_NULL:
                case Value::VALUE_LIST:
                    output->SetNumber(1);

                    return true;
                default:
                    UNREACHABLE(return false);
            }
        default:
            UNREACHABLE(return false);
    }
}

bool Interp::InterpFactor( Value* output ) {
    switch( scanner_.lexme().token ) {
        case TK_ADD:
        case TK_SUB:
        case TK_NOT:
            return InterpUnary(output);
        default:
            return InterpAtomic(output);
    }
}

bool Interp::InterpTerm( Value* output ) {
    if( !InterpFactor(output) )
        return false;
    do {
        TokenId op;
        Value rhs;

        switch( scanner_.lexme().token ) {
            case TK_MUL:
            case TK_DIV:
                op = scanner_.lexme().token;
                scanner_.Move();
                break;
            default:
                return true;

        }

        if( !InterpFactor(&rhs) )
            return false;

        if( output->type() != Value::VALUE_NUMBER ||
                rhs.type() != Value::VALUE_NUMBER ) {
            ReportError("* / can only be used with operand number");
            return false;
        }

        if( op == TK_MUL ) {
            output->SetNumber( output->GetNumber() * rhs.GetNumber() );
        } else {
            if( rhs.GetNumber() == 0 ) {
                ReportError("Divide zero!");
                return false;
            }
            output->SetNumber( output->GetNumber() / rhs.GetNumber() );
        }
    } while(true);

}

bool Interp::InterpComp( Value* output ) {
    if( !InterpTerm(output) )
        return false;

    do {
        TokenId op;
        Value rhs;

        switch( scanner_.lexme().token ) {
            case TK_ADD:
            case TK_SUB:
                op = scanner_.lexme().token;
                scanner_.Move();
                break;
            default:
                return true;
        }

        if( !InterpTerm(&rhs) )
            return false;


        if( output->type() != Value::VALUE_NUMBER ||
            rhs.type() != Value::VALUE_NUMBER ) {
            ReportError("+ - can only work with number operand");
            return false;
        }

        if( op == TK_ADD ) {
            output->SetNumber( output->GetNumber() + rhs.GetNumber() );
        } else {
            output->SetNumber( output->GetNumber() - rhs.GetNumber() );
        }

    } while(true);

}

#define _DO(tk,T) do {\
    switch(tk) { \
        case TK_LT: output->SetNumber( output->Get##T() < rhs.Get##T() ? 1 : 0 ); break; \
        case TK_LET: output->SetNumber( output->Get##T() <= rhs.Get##T() ? 1 : 0 ); break; \
        case TK_GT: output->SetNumber( output->Get##T() > rhs.Get##T() ? 1 : 0 ); break; \
        case TK_GET: output->SetNumber( output->Get##T() >= rhs.Get##T() ? 1 : 0 ); break; \
        case TK_EQ: output->SetNumber( output->Get##T() == rhs.Get##T() ? 1 : 0 ) ; break; \
        case TK_NEQ: output->SetNumber( output->Get##T() != rhs.Get##T() ? 1 : 0 ); break; \
        default: UNREACHABLE(return false); \
    } } while(0)


bool Interp::InterpLogic( Value* output ) {
    if( !InterpComp( output ) )
        return false;

    do {
        TokenId op;
        Value rhs;

        switch( scanner_.lexme().token ) {
            case TK_LT:
            case TK_LET:
            case TK_GT:
            case TK_GET:
            case TK_EQ:
            case TK_NEQ:
                op = scanner_.lexme().token;
                scanner_.Move();
                break;
            default:
                return true;
        }

        if( !InterpComp(&rhs) )
            return false;

        // Do comparison logic for string and number separately.
        // We don't convert string to number automatically by default.

        if( rhs.type() == Value::VALUE_STRING ) {
            if( output->type() != Value::VALUE_STRING ) {
                ReportError("String can only compared to string");
                return false;
            }
            _DO(op,String);
        } else if( rhs.type() == Value::VALUE_NUMBER ) {
            if( output->type() != Value::VALUE_NUMBER ) {
                ReportError("Number can only compared to number");
                return false;
            }

            _DO(op,Number);
        } else {
            ReportError("Only string/number can do comparison!");
            return false;
        }
    } while(true);
}

#undef _DO

bool Interp::InterpTenery( Value* output ) {
    if( !InterpLogic(output) )
        return false;
    do {
        TokenId op;
        Value rhs;

        switch( scanner_.lexme().token ) {
            case TK_AND:
            case TK_OR:
                op = scanner_.lexme().token;
                scanner_.Move();
                break;
            default:
                return true;
        }

        if( !InterpLogic(&rhs) )
            return false;

        assert( output->type() != Value::VALUE_NULL &&
                rhs.type() != Value::VALUE_NULL );

        if( op == TK_AND ) {
            if( output->type() == Value::VALUE_NUMBER ) {
                if( output->GetNumber() == 0 ) {

                output->SetNumber(0);
                    continue;
                }
            }

            if( rhs.type() == Value::VALUE_NUMBER ) {
                if( rhs.GetNumber() == 0 ) {
                    output->SetNumber(0);
                    continue;
                }
            }

            output->SetNumber(1);

        } else {
            if( output->type() == Value::VALUE_NUMBER ) {
                if( output->GetNumber() != 0 ) {
                    output->SetNumber(1);
                    continue;
                }
            }
            if( rhs.type() == Value::VALUE_NUMBER ) {
                if( rhs.GetNumber() != 0 ) {
                    output->SetNumber(1);
                    continue;
                }
            }
            output->SetNumber(0);
        }
    } while(true);
}

bool Interp::InterpPostExp( Value* output ) {
    if( !InterpTenery(output) )
        return false;
    if( scanner_.lexme().token == TK_QUESTION ) {
        // We meet the \"?\" therefore we can be sure that it is a
        // tenery expression here.
        scanner_.Move();
        // We have to evaluate the both expression otherwise we need
        // to skip the other block
        Value l , r;
        if( !InterpExp(&l) )
            return false;

        if( scanner_.lexme().token != TK_COLON ) {
            ReportError("Tenery expression requires \":\"");
            return false;
        }
        scanner_.Move();

        if( !InterpExp(&r) )
            return false;

        if( ToBool(*output) ) {
            *output = l;
        } else {
            *output = r;
        }
    }
    return true;
}

bool Interp::InterpExp( Value* output ) {
    // Post expression is as simple as a {} body. It contains
    // a single line expression and it optionally can have a
    // dollar variable. This dollar variable will smartly expand
    // to context value that resides on its left side . Eg:
    // [1,2,3,4] { $*2 } will make the output to [2,4,6,8].
    if( !InterpPostExp(output) )
        return false;

    if( scanner_.lexme().token == TK_LBRA ) {
        ValueList* new_list = new ValueList();

        // Post Expression
        scanner_.Move();

        // Now set up the context value based on the type of the output value
        if( output->type() == Value::VALUE_LIST ) {
            // Foreach semantic goes here
            const ValueList& l = output->GetList();
            // Remeber the current scanner position, since after each loop
            // we need to rewind back to where we are now
            int pos = scanner_.position();
            int end ;

            // Checking the end of the expression _ONLY_ once
            bool check_end = true;

            for( std::size_t i = 0 ; i < l.size() ; ++i ) {
                dollar_value_ = &l.Index(i);
                Value new_val;

                if(!InterpExp(&new_val)) {
                    delete new_list;
                    return false;
                }

                if( check_end ) {
                    // Checking if we meet that } to indicate the end of the
                    // expression body
                    if( scanner_.lexme().token != TK_RBRA ) {
                        delete new_list;
                        ReportError("Post expression needs } to close the body");
                        return false;
                    }
                    // Move the scanner to set the correct end position
                    scanner_.Move();
                    end = scanner_.position();

                    // Run only once
                    check_end = false;
                }

                // Now we have a new value here
                new_list->AddValue(new_val);
                // Rewind back to where we start to interpret the small value
                scanner_.Set(pos);
            }

            // Set the scanner to the correct position for new token
            scanner_.Set(end);
            // Change the output
            output->SetList(new_list);
            // We definitly have already checked the end of the body, so just return
            return true;
        } else {
            Value new_val;

            // Scalar value
            dollar_value_ = output;
            if( !InterpExp(&new_val) )
                return false;
            // Checking the end of body
            if( scanner_.lexme().token != TK_RBRA ) {
                ReportError("Post expression needs } to close the body");
                return false;
            }
            scanner_.Move();

            *output = new_val;
            return true;
        }
    }
    return true;
}

#ifndef NDEBUG
void TestScanner() {
    std::string txt = "(),+-*/ ><>=>===!= ! && ||";
    Scanner scanner(txt,0);

    do {
        Lexme tk = scanner.Next();
        if (tk.token == TK_EOF)
            break;
        std::cout<< GetTokenName(tk.token) << std::endl;
        scanner.Move();

    } while(true);
    std::cout<<"We are done!"<<std::endl;
}

class TestContext : public Context {
public:

    virtual bool GetVariable( const std::string& var , Value* val ) {
        assert(var == "abcd");
        val->SetNumber(5);
        return true;
    }

    virtual bool ExecFunction( const std::string& func_name ,
                               const std::vector<Value>& par ,
                               Value* ret ,
                               std::string* error ) {
        assert( func_name == "func" );
        ret->SetNumber( par[0].GetNumber() + 1 );
        return true;
    }

};

void TestInterp() {
    std::string txt = "[1..3]{$+10}";
    std::string err;
    int cur_pos;
    Value ret;
    TestContext context;

    Interp interp(
        txt,
        0,
        &context,
        &err);

    assert( interp.DoInterp(&ret,&cur_pos) );
    std::cout<<txt.substr(cur_pos)<<std::endl;
    std::cout<<ret.GetList().Index(1).GetNumber()<<std::endl;
}
#endif // NDEBUG

}// namespace exp
}// namespace

namespace tsub {

using exp::Interp;

ValueList* Value::CopyList( const ValueList& l ) {
    ValueList* ret = new ValueList();

    for( std::size_t i = 0 ; i < l.size() ; ++i ) {
        const Value& value = l.Index( static_cast<int>(i) );
        ret->AddValue(value);
    }

    return ret;
}

class TextProcessor {
private:
    // Manipulate each string as reference inside of the string pool
    typedef std::vector< const std::string* > StrRep;

public:
    TextProcessor( const std::string& input , Context* context , std::string* error_desp ):
        input_(&input),
        context_(context),
        error_desp_(error_desp),
        position_(0)
        {}

    bool Run( std::vector<std::string>* output );

private:
    bool ProcessExp( Value* val );
    void Expand( const std::string* str );
    void Concatenate( const std::vector<const std::string*>& slist );
    void GenerateResult( std::vector<std::string>* output );
    void JoinString( const StrRep& rep , std::string* output );
    void ReportError( const char* format , ... );

    void ValueToStringList( const Value& val , std::vector<const std::string*>* output );
    const std::string* NumberToString( int num );

private:
    const std::string* GetString( const std::string& str ) const {
        std::set<std::string>::iterator ib = str_pool_.find(str);
        if( ib == str_pool_.end() ) {
            // Do real insertion here
            std::pair<
                std::set<std::string>::iterator,
                bool > ret = str_pool_.insert(str);
            assert( ret.second );
            return &(*ret.first);
        } else {
            return &(*ib);
        }
    }

    bool IsEscapeChar( int cha ) {
        switch(cha) {
            case '\\':
            case '`' :
                return true;
            default:
                return false;
        }
    }

private:
    // Intermediate representation of each result set
    std::vector<StrRep> result_set_;

    // Real string pool
    mutable std::set<std::string> str_pool_;

    // Input string pointer
    const std::string* input_;

    // Context
    Context* context_;

    // Error
    std::string* error_desp_;

    // Position
    std::string::size_type position_;
};

void TextProcessor::ReportError( const char* format , ... ) {
    char msg[1024];
    va_list vlist;
    std::stringstream formatter;

    va_start(vlist,format);
    vsprintf(msg,format,vlist);

    formatter<<"[Module:TextProcessor]:"<<msg;
    *error_desp_ = formatter.str();
}

const std::string* TextProcessor::NumberToString( int num ) {
    char buf[256];
    sprintf(buf,"%d",num);
    return GetString(std::string(buf));
}

void TextProcessor::ValueToStringList( const Value& val , std::vector<const std::string*>* output ) {
    switch(val.type()) {
        case Value::VALUE_STRING:
            output->push_back( GetString(val.GetString()) );
            return;
        case Value::VALUE_NUMBER:
            output->push_back( NumberToString(val.GetNumber()) );
            return;
        case Value::VALUE_LIST: {
            const ValueList& vl = val.GetList();
            output->reserve( output->size() + vl.size() );
            for( std::size_t i = 0 ; i < vl.size() ; ++i ) {
                const Value& v = vl.Index(i);
                ValueToStringList(v,output);
            }
            return;
        }
        default:
            UNREACHABLE(return);
    }
}

void TextProcessor::Expand( const std::string* str ) {
    if( result_set_.empty() ) {
        result_set_.push_back(StrRep());
        result_set_.back().push_back(str);
    } else {
        for( std::vector<StrRep>::iterator ib = result_set_.begin() ;
             ib != result_set_.end() ; ++ib ) {
             (*ib).push_back(str);
        }
    }
}

void TextProcessor::Concatenate( const std::vector<const std::string*>& slist ) {
    if( result_set_.empty() ) {
        result_set_.reserve( result_set_.size() + slist.size() );
        // The result set is empty, just copy
        for( std::size_t i = 0 ; i < slist.size() ; ++i ) {
            result_set_.push_back(StrRep());
            result_set_.back().push_back( slist[i] );
        }
    } else {
        // We need to concatenation which will result in set has size:
        // size(slist) * size(result_set_)
        std::vector<StrRep> temp;
        temp.reserve( result_set_.size()*slist.size() );

        for( std::size_t i = 0 ; i < slist.size() ; ++i ) {
            for ( std::vector<StrRep>::iterator ib = result_set_.begin() ;
                  ib != result_set_.end() ; ++ib ) {
                // Constructing a StrRep that is the copy of the element of result_set
                temp.push_back(StrRep(*ib));
                // Push back the new string representation
                temp.back().push_back(slist[i]);
            }
        }

        result_set_.swap(temp);
    }
}

void TextProcessor::JoinString( const StrRep& rep , std::string* output ) {
    std::size_t cap=0;
    assert( !rep.empty() );

    for( StrRep::const_iterator ib = rep.begin() ; ib != rep.end() ; ++ib ) {
        cap += (*ib)->size();
    }
    output->clear();
    output->reserve(cap);

    for( StrRep::const_iterator ib = rep.begin() ; ib != rep.end() ; ++ib ) {
        output->append( *(*ib) );
    }
}


void TextProcessor::GenerateResult( std::vector<std::string>* output ) {
    output->clear();
    output->reserve( result_set_.size() );

    for( std::vector<StrRep>::iterator ib = result_set_.begin() ;
         ib != result_set_.end() ; ++ib ) {
        // For each result set you need to do concatenation
        output->push_back(std::string());
        JoinString( *ib , &(output->back()) );
    }
}


bool TextProcessor::ProcessExp( Value* val ) {
    int new_pos;

    exp::Interp interp( *input_ ,
        static_cast<int>(position_) ,
        context_ ,
        error_desp_ );

    // Now interpreting the script from current position
    if( !interp.DoInterp(val,&new_pos) ) {
        return false;
    }

    if( input_->at(new_pos) != '`' ) {
        ReportError("The expression needs to be ended with \"`\"");
        return false;
    }

    // We don't need to move this cursor, since in the mainloop the ` will
    // be skipped by the main loop counter
    position_ = static_cast<std::size_t>(new_pos);

    return true;
}


bool TextProcessor::Run( std::vector<std::string>* output ) {
    std::string segment;

    // The run loop is simple, it just tries to read the text as long as possible
    // Once it finds a expression , then it executes that one gets the result and
    // then converts it to the string test , this process is called expansion.
    // Then it goes back to find the text.

    for( position_ = 0 ; position_ < input_->size() ; ++position_ ) {
        if( input_->at(position_) == '\\' ) {
            // Handle the escape characters in the stream
            if( position_+1 < input_->size() ) {
                if( IsEscapeChar( input_->at(position_+1) ) ) {
                    ++position_;
                    segment.push_back( input_->at(position_) );
                    continue;
                }
            }
        } else {
            if( input_->at(position_) == '`' ) {
                ++position_;

                Value val;
                std::vector<const std::string*> str_list;

                // We need to put the segment that we currently have to the
                // intermediate result sets now.

                if( !segment.empty() ) {
                    Expand( GetString(segment) );
                    segment.clear();
                }

                if( !ProcessExp(&val) )
                    return false;

                // Convert value to string list
                ValueToStringList(val,&str_list);

                // Once we have the expression, we need to do concatenation
                Concatenate(str_list);

                // Loop again
                continue;
            } else {
                // Just common text, put them into the segment buffer
                segment.push_back( input_->at(position_) );
            }
        }
    }

    // Checking if the segment buffer has something we need to expand
    if( !segment.empty() ) {
        Expand( GetString(segment) );
    }

    // Now generate the result
    GenerateResult( output );
    return true;
}

// Main text processing part
// The special character ` is used to encapsulate the small expression
// for execution. After execution, the output value will be converted
// into the value set and concatenate with the existed text segment .

bool Run( Context* context ,
    const std::string& input ,
    std::vector<std::string>* output,
    std::string* error_desp ) {

    TextProcessor processor(
        input,context,error_desp);

    return processor.Run( output );
}

}// namespace tsub

#ifndef NDEBUG
int main() {
    using tsub::Run;
    std::string error;
    std::vector<std::string> output;

    assert( Run(NULL,
            "c\\``[ 1==1 ? 2:3 ..5 , 1{$*100}]`.http",
            &output,
            &error) );

    for( std::vector<std::string>::iterator ib = output.begin() ;
         ib != output.end() ; ++ib ) {
        std::cout<<*ib<<std::endl;
    }

    return 0;
}
#endif


