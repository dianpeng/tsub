Simple text substituion library -- TSUB

1. Introduction

This small C++ library provides you the ability to do complex string substitution. The substitution
commands are embeded inside of the input text. It accepts one input string and generate multiple 
output strings.

2. Basic Example

Input: http:\\`[1+3*4,5]{$-1}``[2..5]`.com

The input is just a string, the commands is embeded inside of "`" and closed by "`", if the 
string contains such character, you could use "\" to escape it. Each single commands must be
enclosed inside of ` and `. Do not put mutiple commands inside of a single pair of `. 

The command is a small expression language. It has 3 types by default, string literal , number
and list. String literal is same as C/C++ , quoted and allow escape character inside of it.
Number is just integer value (No floating point is supported). And list is a just a array of 
value. The user has no way to access the array's component in it, indeed array will be expanded.

Some simple example:
12 --> Number
"I am a string" --> String
[123,"asd",456] --> List

Pay attention, altough you can , put recursive list [[1,2,3],4] ; however this array will be flatten
to [1,2,3,4] at last.

The command supports alrithmetic operations. Therefore a 1+3 will be evaluated to 4, and 1+3*2 will be
evaluated to 7. 

Generally, the command support nearly all C arithemtic feature, including logic combination, comparison,
arithematic operation and even tenery operation. However, list type doesn't support any operation. It is
sololy for the later text expansion. 

One special operation has been added, call it post processing. For each value or operation, you could 
optionally add a { } body which has a single expression. That expression will process the value and 
mutate its value. 

Therefore a 3{$+10} will be evaluated to 13 , instead of 3. A list can also have such feaature, it is 
only operation that it could use. [1,2,3] { $*2 } will be evalauted to [2,4,6] .

Now, let's see [1+3*4,5]{$-1} evaluated result. As you see, the first part of array is evaluated to
[13,5] , then the post body mutate the array to the [12,4]. Therefore the result is [12,4].

Inside of the list, you could specify the element through 2 ways, one is using comma separate, the other
is using range operator. The range operator _ONLY_ supports number type.

[1,2,3] --> [1,2,3]
[1..4]  --> [1,2,3]
[1*2..3*2] -->[2,3,4,5]

Look at our example's second part of command:[1..4], it will be evaluated to [1,2,3].

Now let's come back to the text subsitution part, the example , after the evaluation of the expression, 
is esentially http://[12,4][1,2,3].com . Then TSUB will convert it to a list of string instead of one. 
This pattern actually has 2 lists, one has 2 elements, the other has 3 elements. The output will have 2*3,
which are 6 strings. 

The rule for composing the value is picking up the value in each array and do combination.

Therefore the result of this string substitution is:
    http://121.com
    http://122.com
    http://123.com
    http://41.com
    http://42.com
    http://43.com

3. Advanced

TSUB do allow the user to register function and variable that user could reference through their subsituion
pattern in C++. Suppose, user registers a reference called a has a value 2.

Then the pattern: http:`a`.com will be evaluated to http:2.com . Also function invoking is allowed as well,
suppose a function called mul will simply multiply its 2 parameters. 
Then pattern http:`mul(1,2+3)`.com will be evaluated to http:5.com

To enable this feature, user just need to implement Context class, here is a possible example :

```
class MyContext : public tsub::Context {
public:
    virtual bool GetVariable( const std::string& variable , Value* output ) {
        if( variable == "a" ) {
            output->SetNumber(1);
            return true;
        } else {
            return false;
        }
    }

    virtual bool ExecFunction( const std::string& func_name , 
                               const std::vector<Value>& pars,
                               Value* ret,
                               std::string* error_descp ) {
        if( func_name == "mul" ) {
            if( pars.size() != 2 ) {
                error_descp->assign("mul function must have 2 parameters");
                return false;
            } else {
                // You should check the type of parameters as well, here just for simplicity
                ret->SetNumber( pars[0].GetNumber()*pars[1].GetNumber() );
                return true;
            }
        } else {
            return false;
        }
    }

};
```

Then you just need to pass this context instance to function run, then all the variable reference
and function invoking will be direct to your context implementation

Have fun :)


