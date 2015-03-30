#ifndef TSUB_H_
#define TSUB_H_

#include <iostream>
#include <string>
#include <vector>
#include <cassert>

namespace tsub {

class ValueList;
class Value {
public:
    enum {
        VALUE_STRING,
        VALUE_NUMBER,
        VALUE_LIST,
        VALUE_NULL
    };

    explicit Value( const std::string& str ):
        type_( VALUE_STRING ) {
            ::new (GetStringPtr()) std::string(str);
        }

    explicit Value( int val ):
        type_( VALUE_NUMBER ) {
            *GetNumberPtr() = val;
        }

    explicit Value( ValueList* l ):
        type_( VALUE_LIST ) {
            buffer_.value_list = l;
        }

    explicit Value( const ValueList& l ):
        type_( VALUE_LIST ) {
            buffer_.value_list = CopyList(l);
        }

    Value() :
        type_( VALUE_NULL )
        {}

    Value( const Value& val ):
        type_(val.type_) {
            CopyHolder(val);
        }

    Value& operator = ( const Value& val ) {
        if( &val == this )
            return *this;
        Detach();
        type_ = val.type_;
        CopyHolder(val);
        return *this;
    }

    ~Value() {
        Detach();
    }

    void SetList( ValueList* list ) {
        Detach();
        type_ = VALUE_LIST;
        buffer_.value_list = list;
    }

    void SetString( const std::string& str ) {
        Detach();
        type_ = VALUE_STRING;
        ::new (GetStringPtr()) std::string(str);
    }

    void SetNumber( int val ) {
        Detach();
        type_ = VALUE_NUMBER;
        *GetNumberPtr() = val;
    }

    void SetNull() {
        Detach();
        type_ = VALUE_NULL;
    }

public:
    int type() const {
        return type_;
    }

    const std::string& GetString() const {
        assert( type_ == VALUE_STRING );
        return *GetStringPtr();
    }

    int GetNumber() const {
        assert( type_ == VALUE_NUMBER );
        return *GetNumberPtr();
    }

    const ValueList& GetList() const {
        assert( type_ == VALUE_LIST );
        return *buffer_.value_list;
    }

    bool IsNull() const {
        return type_ == VALUE_NULL;
    }

private:

    ValueList* CopyList( const ValueList& l );

    void CopyHolder( const Value& val ) {
        switch( val.type_ ) {
            case VALUE_STRING:
                ::new (GetStringPtr()) std::string(val.GetString());
                return;
            case VALUE_NUMBER:
                *GetNumberPtr() = val.GetNumber();
                return;
            case VALUE_LIST:
                buffer_.value_list = CopyList( val.GetList() );
                return;
            default:
                return;
        }
    }

    std::string* GetStringPtr() {
        return reinterpret_cast<std::string*>(
            buffer_.string_buffer);
    }

    const std::string* GetStringPtr() const {
        return reinterpret_cast<const std::string*>(
            buffer_.string_buffer);
    }

    int* GetNumberPtr() {
        return &(buffer_.number_buffer);
    }

    const int* GetNumberPtr() const {
        return &(buffer_.number_buffer);
    }

    inline void Detach();

private:

    union {
        char string_buffer[sizeof(std::string)];
        int number_buffer;
        ValueList* value_list;
    } buffer_;

    int type_;
};

// Value list class, it is a thin wrapper around std::vector<Value>
// The reason why I need this is that I need to decouple the decalaration
// for C++ value type;

class ValueList {
public:
    ValueList(){}
    // Add the value at the back of the list
    void AddValue( const std::string& val ) {
        list_.push_back( Value(val) );
    }

    void AddValue( int val ) {
        list_.push_back( Value(val) );
    }

    void AddValue( const Value& val ) {
        list_.push_back(val);
    }

    // Delete the value from the back of the list
    void DelValue();

    std::size_t size() const {
        return list_.size();
    }

    const Value& Index( int index ) const {
        return list_[index];
    }

    Value& Index( int index ) {
        return list_[index];
    }

    void Clear() {
        list_.clear();
    }

private:
    std::vector< Value > list_;

    ValueList( const ValueList& );
    ValueList& operator = ( const ValueList& );
};

inline void Value::Detach() {
    if( type_ == VALUE_STRING ) {
        using std::string;

        std::string* str = GetStringPtr();
        str->~string();
        return;

    } else if( type_ == VALUE_LIST ) {
        delete buffer_.value_list;
        return;
    }
}

class Context {
public:

    virtual bool GetVariable( const std::string& var, Value* val ) = 0;
    virtual bool ExecFunction( const std::string& name,
                               const std::vector<Value>& par,
                               Value* ret,
                               std::string* error ) =0;

    virtual ~Context() {}
};

bool Run( Context* ctx ,
        const std::string& input,
        std::vector<std::string>* output,
        std::string* error_description );

}// namespace tsub

#endif // TSUB_H_

