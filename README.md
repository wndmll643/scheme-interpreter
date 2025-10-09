# Scheme Interpreter
## Directory Explanation
```
scheme_interpreter/
├── docs/                   report of the project
├── src/                    source index for the scheme interpreter
│   ├── builtin.h           builtin functions are defined here
│   ├── interpreter.cpp     the main function is defined here
│   ├── node.h              the structure node_t and hash_node_t defined
│   ├── table.cpp           
│   ├── table.h             class node array and hash table defined
│   ├── tokenizer.cpp
│   └── tokenizer.h         Read, Print method defined
├── build/                  directory to build the executable
├── Makefile
└── README.md
```
## How to run
```bash
# cleanup the directory
make clean
# compile all of the source files
make all
# after compilation, the executable would be placed at the project directory, and other object files would be placed at the build directory
./interpreter
```
###### 전기정보공학부 2020-19290 이신우