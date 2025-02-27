# Contributing Guidelines
If you have improvements, new backends support, or new operators to contribute to the generic sycl backend, please send us your pull requests! For getting started, see GitHub [howto](https://help.github.com/en/articles/about-pull-requests).

## Pull Request Checklist

Before sending your pull requests, ensure that you follow this checklist:

* Ensure that your code includes proper documentation.

* Ensure that the changes are consistent with the [coding style](CONTRIBUTING.md#coding-style).

## Bug Reporting

If you find a bug or problem, please open a request under [Issues](https://github.com/uxlfoundation/oneMath/issues).

## Security Issues

Report security issues to security@uxlfoundation.org

## Coding Style

The general principle is to follow the style of existing/surrounding code. If you are in doubt, use the `clang-format`:
```sh
clang-format -style=file -i foo.cpp
```
This formats code using the `.clang_format` file found in the generic-sycl-components top-level directory.


### GN: General Naming
* **GN1:** Use snake_case for all type names: classes, structures, enums, template type arguments, type aliases.

* **GN2:** Use snake_case for all variables (global, local, files, function parameters), global and local constants (including constexpr), functions (member, non-member) and enum values.

* **GN3:** Use capitalized SNAKE_CASE only for
macros.

### GF: General Formatting
* **GF1:** Each line of text in the code shall be at most 100 characters long.

* **GF2:** Use only spaces, and indent 4 spaces at a time, never use tabs.

* **GF3:** The open curly brace is always on the end of the last line of the statement (type, function, namespace declaration or control flow statement), not the start of the next line.
```c
int foo() { // <-- curly brace here
    do_something();
}

if (condition) { // <-- curly brace here
    do_something();
}
else { // <-- curly brace here
    do_something();
}

if (condition) { // <-- curly brace here
    do_something();
} else { // <-- Also possible
    do_something();
}
```

* **GF4:** There is never a space between the parentheses and the parameters in function declaration/invocation or control flow statements.

```c
// Wrong
int foo( int arg_1, float arg_2 );
if ( condition );
call_foo( value_1, value_2 );
for ( int i = 0; i < loop_count; i++ );

// Right
int foo(int arg_1, float arg_2);
if (condition);
call_foo(value_1, value_2);
for (int i = 0; i < loop_count; i++);
```

### FA: Files
* **FA1:** Filenames should be lowercase and can include underscores "_".

* **FA2:** C++ header files exposed to the user should end in .hpp.

* **FA3:** C++ source files should end in .cpp.

* **FA4:** All header files shall start with `#pragma once` guards to prevent multiple inclusion, refer to [Structure of Header Files](CONTRIBUTING.md#structure-of-header-files) for more details.

* **FA5:** Each header file shall contain items in the following order:
  1. Copyright
  2. Single blank line
  3. Preprocessor guard
  4. Single blank line
  5. Include statements (if there)
  6. Single blank line if include statements are present
  7. Global macros* (if any)
  8. Single blank line if macros statements are present
  9. Type/function declarations wrapped into namespaces

Note: It is not necessary to put all macro definitions here. Sometimes it is convenient to have macros closer to the place where they are used. For example, sometimes it makes more sense to define macros inside the functions that use them (see Macros for more details). However, if the macro is used throughout the library, put it in header file between includes and the namespace declaration.

* **FA6:** Each header file shall include other header
files in the following order:

  1. C standard headers
  2. C++ standard headers
  3. Single blank line if C/C++ headers are present
  4. Third party libraries' header files (e.g., SYCL, TBB, OMP, etc.)
  5. Single blank line if third party headers are present
  6. Project's header files

### NS: Namespaces
* **NS1:** Use snake_case: all lowercase, with underscores "_" between words for all namespaces.

* **NS2:** Do not indent content inside a namespace scope.

```c
// Wrong! Do not indent
namespace oneapi {
namespace math {

   class table { };

} // namespace math
} // namespace oneapi

// Right
namespace oneapi {
namespace math {

class table { };

} // namespace math
} // namespace oneapi
```

*  **NS4:** Put each namespace on its own line when declaring nested namespaces.

```c
#include "oneapi/math/blas/path_to_some_header.hpp"

namespace oneapi {
namespace math {
namespace blas {

/* ... */

} // namespace blas
} // namespace math
} // namespace oneapi
```


### FU: Functions

*  **FU1:** Use snake_case: all lowercase, with
```c
// Underscores between words for all function names.
return_type class_name::function_name(type_1 arg_name_1, type_2 arg_name_2) {
    do_something();
}
```

*  **FU2:** There is never a space between the function name (or operator) and the open brace. This rule applies to both function declaration/definitions and calls.

Declaration
```c
// Wrong
void foo (type arg_name);
void operator() (type arg_name);
void operator bool ();

// Right
void foo(type arg_name);
void operator()(type arg_name);
void operator bool();
```

Call
```c
// Wrong
const auto x = foo (arg_1, arg_2);

// Right
const auto x = foo(arg_1, arg_2);
```

*  **FU3:** Do not put the function signature and the body on the same line. The only exception is an empty body, in that case place the curly braces at the same line (see rule FU4).
```c
// Wrong
std::int32_t get_something() const { return something_; }

// Right
std::int32_t get_something() const {
    return something_;
}
```

*  **FU4:** Empty function body shall be at the same line as function signature.
```c
// Wrong
void empty_foo(type arg) {
}

// Right
void empty_foo(type arg) {}
```


### CS: Classes and Structures

*  **CS1:** Use snake_case: lower case and all words are separated with underscore character (_).
```c
class numeric_table;
class image;
struct params;
```

*  **CS2:** The acceptable formats for initializer lists are when everything fits on one line:
```c
my_class::my_class(int var) : some_var_(var) {
    do_something();
}
```
If the signature and initializer list are not all on one line, you must line wrap before the colon, indent 8 spaces, put each member on its own line, and align them:
```c
my_class::my_class(int var)
        : some_var_(var),             // <-- 8 space indent
          some_other_var_(var + 1) {  // lined up
    do_something();
}
```
As with any other code block, the close curly brace can be on the same line as the open curly, if it fits:
```c
my_class::my_class(int var)
        : some_var_(var),
          another_var_(0) {}
```


### VC: Variables and Constants

* **VC1:** Use snake_case for all variables, function's arguments and constants.

* **VC2:** Use variables and constant names followed by one underscore "_" for private and protected class-level variables.

* **VC3:** The assignment operator "=" shall be surrounded by single whitespace.
```c
const auto val = get_some_value();
```


### ST: Statements

*  **ST1:** Each of the keywords
if/else/do/while/for/switch shall be followed by one space. An open curly brace after the condition shall be prepended with one space.
```c
while (condition) { // <-- one space after `while` and one space before `{`
    do_something();
} // <-- `;` is not required
```

*  **ST2:** Each of the keywords if/else/do/while/for/switch shall always have accompanying curly braces even if they contain a single-line statement.
```c
// Wrong
if (my_const == my_var)
    do_something();

// Right
if (my_const == my_var) {
    do_something();
}
```

*  **ST3:** The statements within parentheses for operators if, for, while shall have no spaces adjacent to the open and close parentheses characters:
```c
// Wrong
for ( int i = 0; i < loop_size; i++ ) ...;

// Right
for (int i = 0; i < loop_size; i++) ...;
```
